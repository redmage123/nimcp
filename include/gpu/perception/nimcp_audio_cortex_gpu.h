/**
 * @file nimcp_audio_cortex_gpu.h
 * @brief GPU-Accelerated Audio Cortex Processing API
 *
 * WHAT: C API for GPU-accelerated audio processing inspired by the auditory cortex
 * WHY:  GPU acceleration for computationally intensive audio feature extraction
 * HOW:  CUDA kernels for spectral analysis, cochlear modeling, and auditory features
 *
 * BIOLOGICAL BASIS:
 * =================
 * The auditory cortex processes sound through multiple stages:
 * - Cochlea: Frequency decomposition via basilar membrane (tonotopy)
 * - A1 (Primary Auditory Cortex): Spectrotemporal analysis
 * - A2 (Secondary): Complex sound patterns, onset/offset detection
 * - Higher areas: Pitch, rhythm, source localization
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Audio processing benefits from massive parallelism:
 * - FFT/STFT: Parallel across frequency bins and time frames
 * - Filterbanks: Independent filter channels processed simultaneously
 * - Feature extraction: Multiple features computed in parallel
 * - Onset/pitch detection: Frame-parallel processing
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * nimcp_audio_gpu_state_t* audio = nimcp_audio_gpu_create(ctx, 16000, 512, 128, 80, 64);
 *
 * // Compute spectrogram
 * nimcp_gpu_tensor_t* spec = nimcp_audio_gpu_compute_spectrogram(audio, audio_tensor);
 *
 * // Extract full features
 * nimcp_audio_features_gpu_t* features = nimcp_audio_gpu_extract_features(audio, audio_tensor);
 *
 * nimcp_audio_features_gpu_destroy(features);
 * nimcp_audio_gpu_destroy(audio);
 * nimcp_gpu_context_destroy(ctx);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_AUDIO_CORTEX_GPU_H
#define NIMCP_AUDIO_CORTEX_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Forward Declarations
//=============================================================================

/** @brief Opaque GPU audio cortex state */
typedef struct nimcp_audio_gpu_state nimcp_audio_gpu_state_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Cochlear filterbank configuration for GPU
 *
 * Models the frequency decomposition of the basilar membrane using
 * gammatone filters, which approximate cochlear filter responses.
 */
typedef struct nimcp_cochlear_filterbank_gpu {
    nimcp_gpu_tensor_t* center_freqs;      /**< Center frequencies [num_channels] */
    nimcp_gpu_tensor_t* bandwidths;        /**< ERB bandwidths [num_channels] */
    nimcp_gpu_tensor_t* filter_coeffs_b;   /**< IIR numerator coefficients [num_channels, order+1] */
    nimcp_gpu_tensor_t* filter_coeffs_a;   /**< IIR denominator coefficients [num_channels, order+1] */
    int num_channels;                       /**< Number of frequency channels (e.g., 64) */
    int filter_order;                       /**< IIR filter order (typically 4 for gammatone) */
    int sample_rate;                        /**< Audio sample rate */
    float min_freq;                         /**< Minimum center frequency (Hz) */
    float max_freq;                         /**< Maximum center frequency (Hz) */
} nimcp_cochlear_filterbank_gpu_t;

/**
 * @brief Audio GPU processing configuration
 */
typedef struct nimcp_audio_gpu_config {
    int sample_rate;                        /**< Audio sample rate */
    int fft_size;                           /**< FFT window size (power of 2) */
    int hop_size;                           /**< Hop between frames */
    int num_mel_bins;                       /**< Number of mel filterbank channels */
    int num_mfcc;                           /**< Number of MFCC coefficients */
    int num_cochlear_channels;              /**< Number of cochlear filter channels */
    float mel_fmin;                         /**< Mel filterbank minimum frequency */
    float mel_fmax;                         /**< Mel filterbank maximum frequency */
    float cochlear_fmin;                    /**< Cochlear filterbank minimum frequency */
    float cochlear_fmax;                    /**< Cochlear filterbank maximum frequency */
    bool enable_onset_detection;            /**< Enable onset strength computation */
    bool enable_pitch_estimation;           /**< Enable pitch/F0 estimation */
    bool enable_rhythm_features;            /**< Enable tempo/beat tracking */
    bool enable_spatial_audio;              /**< Enable spatial audio (stereo/binaural) */
    float hair_cell_compression;            /**< Hair cell compression exponent (0.3 typical) */
    float lateral_inhibition_strength;      /**< Lateral inhibition factor */
} nimcp_audio_gpu_config_t;

/**
 * @brief GPU audio feature extraction output
 *
 * Contains all extracted audio features computed in parallel on GPU
 */
typedef struct nimcp_audio_features_gpu {
    nimcp_gpu_tensor_t* spectrogram;        /**< Power spectrogram [num_frames, freq_bins] */
    nimcp_gpu_tensor_t* mel_spectrogram;    /**< Mel spectrogram [num_frames, mel_bins] */
    nimcp_gpu_tensor_t* mfcc;               /**< MFCCs [num_frames, num_mfcc] */
    nimcp_gpu_tensor_t* cochleagram;        /**< Cochleagram [num_frames, cochlear_channels] */
    nimcp_gpu_tensor_t* onset_strength;     /**< Onset strength [num_frames] */
    nimcp_gpu_tensor_t* pitch_salience;     /**< Pitch salience [num_frames, pitch_bins] */
    nimcp_gpu_tensor_t* f0_estimate;        /**< F0 estimates [num_frames] */
    nimcp_gpu_tensor_t* f0_confidence;      /**< F0 confidence [num_frames] */
    nimcp_gpu_tensor_t* tempo_estimate;     /**< Tempo curve [tempo_bins] */
    nimcp_gpu_tensor_t* beat_times;         /**< Beat positions [max_beats] */
    int num_frames;                          /**< Number of time frames */
    int num_beats;                           /**< Number of detected beats */
} nimcp_audio_features_gpu_t;

/**
 * @brief Spatial audio features for stereo/binaural signals
 */
typedef struct nimcp_spatial_audio_gpu {
    nimcp_gpu_tensor_t* itd;                /**< Interaural time difference [num_frames] */
    nimcp_gpu_tensor_t* ild;                /**< Interaural level difference [num_frames, freq_bins] */
    nimcp_gpu_tensor_t* gcc_phat;           /**< GCC-PHAT cross-correlation [num_frames, lags] */
    nimcp_gpu_tensor_t* azimuth_estimate;   /**< Estimated azimuth angle [num_frames] */
} nimcp_spatial_audio_gpu_t;

/**
 * @brief Audio GPU processing statistics
 */
typedef struct nimcp_audio_gpu_stats {
    uint64_t frames_processed;              /**< Total frames processed */
    uint64_t fft_operations;                /**< Total FFT operations */
    uint64_t filterbank_operations;         /**< Filterbank applications */
    float avg_stft_time_us;                 /**< Average STFT computation time */
    float avg_mel_time_us;                  /**< Average mel spectrogram time */
    float avg_onset_time_us;                /**< Average onset detection time */
    float avg_pitch_time_us;                /**< Average pitch estimation time */
    size_t gpu_memory_used;                 /**< GPU memory in use */
} nimcp_audio_gpu_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default audio GPU configuration
 *
 * @return Default configuration for 16kHz audio
 */
NIMCP_EXPORT nimcp_audio_gpu_config_t nimcp_audio_gpu_default_config(void);

/**
 * @brief Get configuration for speech processing (16kHz)
 *
 * @return Optimized configuration for speech signals
 */
NIMCP_EXPORT nimcp_audio_gpu_config_t nimcp_audio_gpu_speech_config(void);

/**
 * @brief Get configuration for music processing (44.1kHz)
 *
 * @return Optimized configuration for music signals
 */
NIMCP_EXPORT nimcp_audio_gpu_config_t nimcp_audio_gpu_music_config(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create GPU audio cortex processing state
 *
 * WHAT: Initialize GPU resources for audio processing
 * WHY:  Allocate GPU memory for FFT plans, filterbanks, buffers
 * HOW:  Create cuFFT plans, upload filterbank matrices, allocate working buffers
 *
 * @param ctx GPU context (required)
 * @param sample_rate Audio sample rate in Hz
 * @param fft_size FFT window size (must be power of 2)
 * @param hop_size Hop between frames
 * @param num_mel_bins Number of mel filterbank channels
 * @param num_cochlear_channels Number of cochlear filter channels
 * @return New audio GPU state, or NULL on failure
 */
NIMCP_EXPORT nimcp_audio_gpu_state_t* nimcp_audio_gpu_create(
    nimcp_gpu_context_t* ctx,
    int sample_rate,
    int fft_size,
    int hop_size,
    int num_mel_bins,
    int num_cochlear_channels
);

/**
 * @brief Create GPU audio state with full configuration
 *
 * @param ctx GPU context (required)
 * @param config Full configuration structure
 * @return New audio GPU state, or NULL on failure
 */
NIMCP_EXPORT nimcp_audio_gpu_state_t* nimcp_audio_gpu_create_with_config(
    nimcp_gpu_context_t* ctx,
    const nimcp_audio_gpu_config_t* config
);

/**
 * @brief Destroy GPU audio cortex state
 *
 * @param state Audio GPU state to destroy
 */
NIMCP_EXPORT void nimcp_audio_gpu_destroy(nimcp_audio_gpu_state_t* state);

/**
 * @brief Synchronize GPU audio operations
 *
 * @param state Audio GPU state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_audio_gpu_synchronize(nimcp_audio_gpu_state_t* state);

//=============================================================================
// Spectral Analysis Functions
//=============================================================================

/**
 * @brief Compute power spectrogram (GPU-accelerated)
 *
 * WHAT: Compute magnitude-squared STFT of audio signal
 * WHY:  Spectral representation for audio analysis
 * HOW:  Frame extraction, windowing, cuFFT, magnitude computation
 *
 * @param state Audio GPU state
 * @param audio Input audio tensor [samples] or [batch, samples]
 * @return Power spectrogram tensor [frames, freq_bins] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_compute_spectrogram(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Compute complex STFT (GPU-accelerated)
 *
 * WHAT: Compute complex-valued STFT (magnitude + phase)
 * WHY:  Needed for onset detection, phase-based features
 * HOW:  Frame extraction, windowing, cuFFT
 *
 * @param state Audio GPU state
 * @param audio Input audio tensor
 * @param magnitude Output magnitude tensor (caller owns, can be NULL)
 * @param phase Output phase tensor (caller owns, can be NULL)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_audio_gpu_compute_stft(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t** magnitude,
    nimcp_gpu_tensor_t** phase
);

/**
 * @brief Compute mel spectrogram (GPU-accelerated)
 *
 * WHAT: Apply mel filterbank to power spectrogram
 * WHY:  Perceptually-motivated frequency representation
 * HOW:  GPU matrix multiply with precomputed mel filterbank
 *
 * @param state Audio GPU state
 * @param audio Input audio tensor
 * @return Mel spectrogram tensor [frames, mel_bins] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_compute_mel_spectrogram(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Compute MFCCs (GPU-accelerated)
 *
 * WHAT: Compute Mel-Frequency Cepstral Coefficients
 * WHY:  Compact representation for speech/speaker recognition
 * HOW:  Mel spectrogram -> log -> DCT
 *
 * @param state Audio GPU state
 * @param audio Input audio tensor
 * @param num_mfcc Number of MFCC coefficients to return
 * @return MFCC tensor [frames, num_mfcc] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_compute_mfcc(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    int num_mfcc
);

//=============================================================================
// Cochlear Modeling Functions
//=============================================================================

/**
 * @brief Compute cochleagram (GPU-accelerated)
 *
 * WHAT: Biologically-inspired auditory spectrogram using gammatone filters
 * WHY:  Mimics cochlear frequency decomposition for robust features
 * HOW:  GPU gammatone filterbank, half-wave rectification, compression
 *
 * @param state Audio GPU state
 * @param audio Input audio tensor
 * @return Cochleagram tensor [frames, channels] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_compute_cochleagram(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Initialize gammatone filterbank on GPU
 *
 * WHAT: Create gammatone filterbank coefficients on GPU
 * WHY:  Biologically-accurate cochlear model
 * HOW:  Compute ERB-spaced center frequencies, generate IIR coefficients
 *
 * @param state Audio GPU state
 * @param num_channels Number of filter channels
 * @param min_freq Minimum center frequency (Hz)
 * @param max_freq Maximum center frequency (Hz)
 * @return Filterbank structure (caller owns)
 */
NIMCP_EXPORT nimcp_cochlear_filterbank_gpu_t* nimcp_audio_gpu_create_gammatone_filterbank(
    nimcp_audio_gpu_state_t* state,
    int num_channels,
    float min_freq,
    float max_freq
);

/**
 * @brief Destroy cochlear filterbank
 *
 * @param filterbank Filterbank to destroy
 */
NIMCP_EXPORT void nimcp_cochlear_filterbank_gpu_destroy(
    nimcp_cochlear_filterbank_gpu_t* filterbank
);

/**
 * @brief Apply cochlear filterbank to audio (GPU-accelerated)
 *
 * @param state Audio GPU state
 * @param filterbank Gammatone filterbank
 * @param audio Input audio tensor
 * @return Filtered output [samples, channels] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_apply_cochlear_filterbank(
    nimcp_audio_gpu_state_t* state,
    nimcp_cochlear_filterbank_gpu_t* filterbank,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Apply hair cell transduction model (GPU-accelerated)
 *
 * WHAT: Model inner hair cell response (half-wave rect + compression)
 * WHY:  Biologically-accurate transduction from mechanical to neural
 * HOW:  GPU kernel for HWR and power-law compression
 *
 * @param state Audio GPU state
 * @param filterbank_output Output from cochlear filterbank
 * @param compression_exp Compression exponent (typically 0.3)
 * @return Transduced output (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_hair_cell_transduction(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* filterbank_output,
    float compression_exp
);

/**
 * @brief Apply lateral inhibition (GPU-accelerated)
 *
 * WHAT: Model lateral suppression between frequency channels
 * WHY:  Enhances spectral contrast like biological auditory system
 * HOW:  GPU convolution across channels with inhibitory kernel
 *
 * @param state Audio GPU state
 * @param input Input cochleagram
 * @param inhibition_strength Strength of lateral inhibition (0-1)
 * @return Output with lateral inhibition (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_lateral_inhibition(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* input,
    float inhibition_strength
);

//=============================================================================
// Onset Detection Functions
//=============================================================================

/**
 * @brief Detect onsets (GPU-accelerated)
 *
 * WHAT: Compute onset strength function and detect onset times
 * WHY:  Identify note/event onsets for rhythm, segmentation
 * HOW:  Spectral flux, peak picking
 *
 * @param state Audio GPU state
 * @param audio Input audio tensor
 * @return Onset strength tensor [frames] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_detect_onsets(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Compute spectral flux onset function (GPU-accelerated)
 *
 * WHAT: Measure spectral change between frames
 * WHY:  Primary onset detection feature
 * HOW:  Half-wave rectified difference of spectrogram frames
 *
 * @param state Audio GPU state
 * @param spectrogram Input spectrogram
 * @param rectify Use half-wave rectification (positive changes only)
 * @return Spectral flux [frames] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_spectral_flux(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* spectrogram,
    bool rectify
);

/**
 * @brief Compute high-frequency content onset function (GPU-accelerated)
 *
 * WHAT: Weight spectral content by frequency
 * WHY:  Emphasizes transients which have more HF energy
 * HOW:  Frequency-weighted sum of spectrogram
 *
 * @param state Audio GPU state
 * @param spectrogram Input spectrogram
 * @return High-frequency content [frames] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_high_freq_content(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* spectrogram
);

/**
 * @brief Compute complex domain onset function (GPU-accelerated)
 *
 * WHAT: Use phase information for onset detection
 * WHY:  More accurate onset timing than magnitude-only
 * HOW:  Phase deviation + magnitude change
 *
 * @param state Audio GPU state
 * @param magnitude Current and previous frame magnitudes
 * @param phase Current and previous frame phases
 * @return Complex onset strength [frames] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_complex_domain_onset(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* magnitude,
    nimcp_gpu_tensor_t* phase
);

/**
 * @brief Pick peaks from onset function (GPU-accelerated)
 *
 * WHAT: Find local maxima in onset strength function
 * WHY:  Convert continuous function to discrete onset times
 * HOW:  GPU parallel peak detection with threshold
 *
 * @param state Audio GPU state
 * @param onset_strength Onset strength function
 * @param threshold Detection threshold (relative to max)
 * @param pre_max Samples before for local max detection
 * @param post_max Samples after for local max detection
 * @return Peak indices tensor (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_peak_pick(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* onset_strength,
    float threshold,
    int pre_max,
    int post_max
);

//=============================================================================
// Pitch Estimation Functions
//=============================================================================

/**
 * @brief Estimate pitch/F0 (GPU-accelerated)
 *
 * WHAT: Fundamental frequency estimation
 * WHY:  Pitch is critical for speech prosody, music
 * HOW:  YIN algorithm on GPU
 *
 * @param state Audio GPU state
 * @param audio Input audio tensor
 * @return F0 estimates [frames] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_estimate_pitch(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Compute pitch autocorrelation (GPU-accelerated)
 *
 * @param state Audio GPU state
 * @param frames Windowed audio frames [num_frames, frame_size]
 * @param max_lag Maximum lag for pitch period search
 * @return Autocorrelation [num_frames, max_lag] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_pitch_autocorr(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* frames,
    int max_lag
);

/**
 * @brief Compute YIN difference function (GPU-accelerated)
 *
 * WHAT: YIN algorithm difference function
 * WHY:  Robust pitch estimation
 * HOW:  Autocorrelation-based difference with normalization
 *
 * @param state Audio GPU state
 * @param frames Windowed audio frames
 * @param max_lag Maximum lag
 * @return YIN difference function [num_frames, max_lag] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_yin_difference(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* frames,
    int max_lag
);

/**
 * @brief Compute pitch salience map (GPU-accelerated)
 *
 * WHAT: Multi-pitch salience function
 * WHY:  Polyphonic pitch estimation, chord detection
 * HOW:  Harmonic sum on spectrogram
 *
 * @param state Audio GPU state
 * @param spectrogram Input spectrogram
 * @param num_harmonics Number of harmonics to sum
 * @param harmonic_decay Decay factor for higher harmonics
 * @return Pitch salience [frames, pitch_bins] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_pitch_salience(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* spectrogram,
    int num_harmonics,
    float harmonic_decay
);

//=============================================================================
// Rhythm and Beat Tracking Functions
//=============================================================================

/**
 * @brief Estimate tempo (GPU-accelerated)
 *
 * WHAT: Estimate tempo in BPM from onset strength
 * WHY:  Rhythm analysis, beat tracking
 * HOW:  Autocorrelation of onset function
 *
 * @param state Audio GPU state
 * @param onset_strength Onset strength function
 * @param min_bpm Minimum tempo (BPM)
 * @param max_bpm Maximum tempo (BPM)
 * @return Estimated tempo in BPM
 */
NIMCP_EXPORT float nimcp_audio_gpu_estimate_tempo(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* onset_strength,
    float min_bpm,
    float max_bpm
);

/**
 * @brief Compute tempo autocorrelation (GPU-accelerated)
 *
 * @param state Audio GPU state
 * @param onset_strength Onset strength function
 * @param min_lag Minimum lag (corresponds to max tempo)
 * @param max_lag Maximum lag (corresponds to min tempo)
 * @return Tempo autocorrelation curve (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_tempo_autocorr(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* onset_strength,
    int min_lag,
    int max_lag
);

/**
 * @brief Track beats using dynamic programming (GPU-accelerated)
 *
 * WHAT: Find optimal beat sequence
 * WHY:  Precise beat positions for rhythm analysis
 * HOW:  DP on GPU with tempo-based transition costs
 *
 * @param state Audio GPU state
 * @param onset_strength Onset strength function
 * @param tempo_bpm Estimated tempo
 * @param beat_times Output beat times (caller allocates, NULL to query size)
 * @param max_beats Maximum beats to return
 * @return Number of beats found
 */
NIMCP_EXPORT int nimcp_audio_gpu_track_beats(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* onset_strength,
    float tempo_bpm,
    float* beat_times,
    int max_beats
);

//=============================================================================
// Spatial Audio Functions
//=============================================================================

/**
 * @brief Compute interaural time difference (GPU-accelerated)
 *
 * WHAT: Estimate ITD from stereo signal
 * WHY:  Sound source localization
 * HOW:  Cross-correlation between channels
 *
 * @param state Audio GPU state
 * @param left Left channel audio
 * @param right Right channel audio
 * @return ITD estimates [frames] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_compute_itd(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* left,
    nimcp_gpu_tensor_t* right
);

/**
 * @brief Compute interaural level difference (GPU-accelerated)
 *
 * WHAT: Estimate ILD from stereo spectrogram
 * WHY:  Sound source localization (especially HF)
 * HOW:  dB difference between channel spectrograms
 *
 * @param state Audio GPU state
 * @param left_spec Left channel spectrogram
 * @param right_spec Right channel spectrogram
 * @return ILD [frames, freq_bins] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_compute_ild(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* left_spec,
    nimcp_gpu_tensor_t* right_spec
);

/**
 * @brief Compute GCC-PHAT cross-correlation (GPU-accelerated)
 *
 * WHAT: Generalized Cross Correlation - Phase Transform
 * WHY:  Robust time delay estimation for localization
 * HOW:  Whitened cross-correlation in frequency domain
 *
 * @param state Audio GPU state
 * @param left_fft Left channel FFT
 * @param right_fft Right channel FFT
 * @return GCC-PHAT [frames, lags] (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_gcc_phat(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* left_fft,
    nimcp_gpu_tensor_t* right_fft
);

/**
 * @brief Extract spatial audio features (GPU-accelerated)
 *
 * WHAT: Complete spatial audio feature extraction
 * WHY:  Sound source localization and separation
 * HOW:  Compute ITD, ILD, GCC-PHAT in parallel
 *
 * @param state Audio GPU state
 * @param left Left channel audio
 * @param right Right channel audio
 * @return Spatial features structure (caller owns)
 */
NIMCP_EXPORT nimcp_spatial_audio_gpu_t* nimcp_audio_gpu_extract_spatial_features(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* left,
    nimcp_gpu_tensor_t* right
);

/**
 * @brief Destroy spatial audio features
 *
 * @param spatial Spatial features to destroy
 */
NIMCP_EXPORT void nimcp_spatial_audio_gpu_destroy(nimcp_spatial_audio_gpu_t* spatial);

//=============================================================================
// Full Feature Extraction
//=============================================================================

/**
 * @brief Extract all audio features (GPU-accelerated)
 *
 * WHAT: Complete audio feature extraction pipeline
 * WHY:  Comprehensive features for audio understanding
 * HOW:  Parallel computation of spectral, onset, pitch, rhythm features
 *
 * @param state Audio GPU state
 * @param audio Input audio tensor
 * @return Complete feature structure (caller owns)
 */
NIMCP_EXPORT nimcp_audio_features_gpu_t* nimcp_audio_gpu_extract_features(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Destroy audio features structure
 *
 * @param features Features to destroy
 */
NIMCP_EXPORT void nimcp_audio_features_gpu_destroy(nimcp_audio_features_gpu_t* features);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Apply window function (GPU-accelerated)
 *
 * @param state Audio GPU state
 * @param frames Audio frames to window [num_frames, frame_size]
 * @param window_type 0=Hann, 1=Hamming, 2=Blackman, 3=Kaiser
 * @return Windowed frames (modifies in-place, returns same pointer)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_apply_window(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* frames,
    int window_type
);

/**
 * @brief Convert power to dB scale (GPU-accelerated)
 *
 * @param state Audio GPU state
 * @param power Power spectrogram
 * @param ref_power Reference power (for 0 dB)
 * @param amin Minimum power (floor)
 * @return dB-scale spectrogram (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_power_to_db(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* power,
    float ref_power,
    float amin
);

/**
 * @brief Compute delta features (GPU-accelerated)
 *
 * WHAT: First derivative of features over time
 * WHY:  Captures dynamics for speech recognition
 * HOW:  Finite difference approximation
 *
 * @param state Audio GPU state
 * @param features Input features [frames, feature_dim]
 * @param width Delta window width
 * @return Delta features (caller owns)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_audio_gpu_delta_features(
    nimcp_audio_gpu_state_t* state,
    nimcp_gpu_tensor_t* features,
    int width
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get audio GPU statistics
 *
 * @param state Audio GPU state
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_audio_gpu_get_stats(
    const nimcp_audio_gpu_state_t* state,
    nimcp_audio_gpu_stats_t* stats
);

/**
 * @brief Reset audio GPU statistics
 *
 * @param state Audio GPU state
 */
NIMCP_EXPORT void nimcp_audio_gpu_reset_stats(nimcp_audio_gpu_state_t* state);

/**
 * @brief Get configuration from audio GPU state
 *
 * @param state Audio GPU state
 * @return Configuration structure (copy)
 */
NIMCP_EXPORT nimcp_audio_gpu_config_t nimcp_audio_gpu_get_config(
    const nimcp_audio_gpu_state_t* state
);

//=============================================================================
// CPU Reference Implementations (for testing)
//=============================================================================

/**
 * @brief CPU reference implementation of mel spectrogram
 */
NIMCP_EXPORT bool nimcp_audio_cpu_mel_spectrogram(
    const float* audio,
    int audio_len,
    float* mel_spec,
    int sample_rate,
    int fft_size,
    int hop_size,
    int num_mel_bins,
    float mel_fmin,
    float mel_fmax
);

/**
 * @brief CPU reference implementation of spectral flux
 */
NIMCP_EXPORT bool nimcp_audio_cpu_spectral_flux(
    const float* spectrogram,
    int num_frames,
    int num_bins,
    float* flux,
    bool rectify
);

/**
 * @brief CPU reference implementation of YIN pitch detection
 */
NIMCP_EXPORT bool nimcp_audio_cpu_yin_pitch(
    const float* audio,
    int audio_len,
    float* f0,
    float* confidence,
    int sample_rate,
    float min_f0,
    float max_f0,
    float threshold
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIO_CORTEX_GPU_H */
