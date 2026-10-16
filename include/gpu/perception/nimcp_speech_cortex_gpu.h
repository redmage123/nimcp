/**
 * @file nimcp_speech_cortex_gpu.h
 * @brief GPU-Accelerated Speech Cortex Processing API
 *
 * WHAT: C API for GPU-accelerated speech processing operations
 * WHY:  GPU acceleration for computationally intensive speech analysis
 * HOW:  CUDA kernels for FFT, MFCC, LPC, formant extraction, pitch detection
 *
 * BIOLOGICAL BASIS:
 * =================
 * Speech processing involves multiple parallel computations:
 * - Spectral analysis (cochlear frequency decomposition)
 * - Formant tracking (vocal tract resonance estimation)
 * - Pitch detection (fundamental frequency extraction)
 * - Phoneme recognition (categorical perception)
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Speech analysis benefits from massive parallelism:
 * - FFT: O(N log N) but highly parallelizable across frequency bins
 * - Mel filterbank: Parallel matrix-vector multiplication
 * - Autocorrelation: Parallelizable across lag values
 * - LPC: Batch processing across multiple frames
 * - Phoneme classification: Parallel softmax across phoneme classes
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * nimcp_speech_gpu_state_t* speech = nimcp_speech_gpu_create(ctx, 16000, 400, 160, 80, 12);
 *
 * // Extract MFCC features
 * nimcp_gpu_tensor_t* mfcc = nimcp_speech_gpu_compute_mfcc(speech, audio);
 *
 * // Detect pitch
 * nimcp_gpu_tensor_t* pitch = nimcp_speech_gpu_detect_pitch(speech, audio);
 *
 * // Extract formants
 * nimcp_gpu_tensor_t* formants = nimcp_speech_gpu_extract_formants(speech, audio);
 *
 * // Full pipeline
 * nimcp_phoneme_result_gpu_t* result = nimcp_speech_gpu_recognize(speech, audio);
 *
 * nimcp_speech_gpu_destroy(speech);
 * nimcp_gpu_context_destroy(ctx);
 * @endcode
 *
 * REFERENCES:
 * - Huang et al. (2001) Spoken Language Processing
 * - Rabiner & Schafer (2007) Theory and Applications of Digital Speech Processing
 * - Davis & Mermelstein (1980) MFCC computation
 * - Markel & Gray (1976) Linear Prediction of Speech
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SPEECH_CORTEX_GPU_H
#define NIMCP_SPEECH_CORTEX_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "perception/nimcp_speech_cortex.h"  // For phoneme_t definition
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Configuration Constants
//=============================================================================

/** Maximum FFT size for speech processing */
#define SPEECH_GPU_MAX_FFT_SIZE 4096

/** Default number of mel filterbank bins */
#define SPEECH_GPU_DEFAULT_MEL_BINS 80

/** Default number of MFCC coefficients */
#define SPEECH_GPU_DEFAULT_NUM_MFCC 13

/** Default LPC order for formant extraction */
#define SPEECH_GPU_DEFAULT_LPC_ORDER 12

/** Number of formants to extract */
#define SPEECH_GPU_NUM_FORMANTS 4

/** Maximum number of pitch candidates per frame */
#define SPEECH_GPU_MAX_PITCH_CANDIDATES 8

/** Number of phoneme classes (English) */
#define SPEECH_GPU_NUM_PHONEMES 44

//=============================================================================
// Window Type Enumeration
//=============================================================================

/**
 * @brief Window function types for STFT analysis
 */
typedef enum {
    SPEECH_GPU_WINDOW_HAMMING = 0,  /**< Hamming window (default for speech) */
    SPEECH_GPU_WINDOW_HANN = 1,     /**< Hann (Hanning) window */
    SPEECH_GPU_WINDOW_BLACKMAN = 2, /**< Blackman window */
    SPEECH_GPU_WINDOW_RECTANGULAR = 3 /**< Rectangular (no window) */
} speech_gpu_window_type_t;

//=============================================================================
// GPU Speech Processing State
//=============================================================================

/**
 * @brief GPU speech processing state
 *
 * WHAT: Manages GPU resources for speech feature extraction
 * WHY:  Centralize FFT plans, filterbanks, and buffers for efficiency
 * HOW:  Pre-allocate and reuse GPU memory across processing calls
 */
typedef struct nimcp_speech_gpu_state {
    nimcp_gpu_context_t* ctx;           /**< GPU context reference */

    // === FFT Configuration ===
#ifdef NIMCP_ENABLE_CUDA
    cufftHandle fft_plan;               /**< cuFFT plan for forward FFT */
    cufftHandle ifft_plan;              /**< cuFFT plan for inverse FFT */
    cufftHandle fft_plan_batch;         /**< Batched FFT plan for multiple frames */
#else
    int fft_plan;                       /**< Placeholder for non-CUDA builds */
    int ifft_plan;
    int fft_plan_batch;
#endif
    int fft_size;                       /**< FFT window size (samples) */
    int fft_bins;                       /**< FFT output bins (fft_size/2 + 1) */
    bool fft_initialized;               /**< FFT plans created flag */

    // === Pre-computed Buffers (GPU memory) ===
    nimcp_gpu_tensor_t* window;         /**< Window function [fft_size] */
    nimcp_gpu_tensor_t* mel_filterbank; /**< Mel filterbank [num_mel_bins, fft_bins] */
    nimcp_gpu_tensor_t* mel_center_freqs; /**< Mel center frequencies [num_mel_bins] */
    nimcp_gpu_tensor_t* dct_matrix;     /**< DCT matrix for MFCC [num_mfcc, num_mel_bins] */
    nimcp_gpu_tensor_t* lpc_buffer;     /**< LPC working buffer */
    nimcp_gpu_tensor_t* autocorr_buffer; /**< Autocorrelation buffer */

    // === Temporary Buffers ===
    nimcp_gpu_tensor_t* fft_buffer;     /**< Complex FFT output buffer */
    nimcp_gpu_tensor_t* power_spectrum; /**< Power spectrum buffer */
    nimcp_gpu_tensor_t* mel_energies;   /**< Mel filterbank energies */
    nimcp_gpu_tensor_t* log_mel;        /**< Log mel energies */
    nimcp_gpu_tensor_t* formant_buffer; /**< Formant extraction buffer */
    nimcp_gpu_tensor_t* pitch_buffer;   /**< Pitch detection buffer */

    // === Configuration Parameters ===
    int sample_rate;                    /**< Audio sample rate (Hz) */
    int frame_size;                     /**< Frame size (samples) */
    int hop_size;                       /**< Hop size between frames (samples) */
    int num_mel_bins;                   /**< Number of mel filterbank bins */
    int num_mfcc;                       /**< Number of MFCC coefficients */
    int lpc_order;                      /**< LPC order for formant extraction */
    int max_frames;                     /**< Maximum frames per batch */
    speech_gpu_window_type_t window_type; /**< Window function type */

    // === Pitch Detection Parameters ===
    float min_pitch_hz;                 /**< Minimum F0 (Hz, default 50) */
    float max_pitch_hz;                 /**< Maximum F0 (Hz, default 500) */
    int min_pitch_lag;                  /**< Min lag (samples) for pitch */
    int max_pitch_lag;                  /**< Max lag (samples) for pitch */

    // === Feature Configuration ===
    float preemphasis_coeff;            /**< Pre-emphasis coefficient (default 0.97) */
    float mel_floor;                    /**< Floor value for log mel (default 1e-10) */
    bool include_energy;                /**< Include energy in MFCC (default true) */
    bool mean_normalize;                /**< Apply mean normalization (CMN) */

    // === Phoneme Classification ===
    nimcp_gpu_tensor_t* phoneme_weights; /**< Phoneme classifier weights */
    nimcp_gpu_tensor_t* phoneme_bias;   /**< Phoneme classifier bias */
    int num_phonemes;                   /**< Number of phoneme classes */
    bool classifier_initialized;        /**< Classifier weights loaded */

    // === Statistics ===
    uint64_t frames_processed;          /**< Total frames processed */
    uint64_t fft_operations;            /**< Total FFT operations */
    float total_processing_time_ms;     /**< Cumulative processing time */
} nimcp_speech_gpu_state_t;

//=============================================================================
// Phoneme Recognition Output
//=============================================================================

/**
 * @brief GPU phoneme recognition result
 *
 * WHAT: Contains per-frame phoneme probabilities and decoded sequence
 * WHY:  Enable downstream language processing and transcription
 * HOW:  Stores probability distribution and argmax decode
 */
typedef struct nimcp_phoneme_result_gpu {
    nimcp_gpu_tensor_t* phoneme_probs;  /**< [num_frames, num_phonemes] probabilities */
    nimcp_gpu_tensor_t* phoneme_ids;    /**< [num_frames] decoded phoneme indices */
    nimcp_gpu_tensor_t* phoneme_confidence; /**< [num_frames] max probability per frame */
    int num_frames;                     /**< Number of output frames */
    int num_phonemes;                   /**< Number of phoneme classes */
} nimcp_phoneme_result_gpu_t;

//=============================================================================
// Feature Extraction Output
//=============================================================================

/**
 * @brief GPU speech feature extraction result
 *
 * WHAT: Comprehensive speech features from audio
 * WHY:  Provide all relevant speech features for downstream processing
 * HOW:  Bundle MFCC, pitch, formants, and energy
 */
typedef struct nimcp_speech_features_gpu {
    nimcp_gpu_tensor_t* mfcc;           /**< MFCC coefficients [num_frames, num_mfcc] */
    nimcp_gpu_tensor_t* delta_mfcc;     /**< Delta MFCC [num_frames, num_mfcc] */
    nimcp_gpu_tensor_t* delta2_mfcc;    /**< Delta-delta MFCC [num_frames, num_mfcc] */
    nimcp_gpu_tensor_t* pitch;          /**< Pitch (F0) per frame [num_frames] */
    nimcp_gpu_tensor_t* pitch_confidence; /**< Pitch detection confidence [num_frames] */
    nimcp_gpu_tensor_t* formants;       /**< Formant frequencies [num_frames, 4] */
    nimcp_gpu_tensor_t* formant_bandwidths; /**< Formant bandwidths [num_frames, 4] */
    nimcp_gpu_tensor_t* energy;         /**< Frame energy [num_frames] */
    nimcp_gpu_tensor_t* zcr;            /**< Zero crossing rate [num_frames] */
    nimcp_gpu_tensor_t* vad;            /**< Voice activity [num_frames] (binary) */
    int num_frames;                     /**< Number of frames extracted */
    int sample_rate;                    /**< Original sample rate */
} nimcp_speech_features_gpu_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief GPU speech processing configuration
 */
typedef struct nimcp_speech_gpu_config {
    int sample_rate;                    /**< Audio sample rate (Hz) */
    int frame_size;                     /**< Frame size in samples */
    int hop_size;                       /**< Hop size in samples */
    int fft_size;                       /**< FFT size (0 = auto from frame_size) */
    int num_mel_bins;                   /**< Mel filterbank bins (default 80) */
    int num_mfcc;                       /**< MFCC coefficients (default 13) */
    int lpc_order;                      /**< LPC order (default 12) */
    int max_frames;                     /**< Max frames per batch (default 1000) */
    speech_gpu_window_type_t window_type; /**< Window function type */
    float preemphasis_coeff;            /**< Pre-emphasis (default 0.97) */
    float min_pitch_hz;                 /**< Min F0 (default 50) */
    float max_pitch_hz;                 /**< Max F0 (default 500) */
    bool include_energy;                /**< Include energy in MFCC */
    bool mean_normalize;                /**< Apply CMN */
} nimcp_speech_gpu_config_t;

/**
 * @brief Get default GPU speech configuration
 *
 * @return Default configuration with typical speech parameters
 */
NIMCP_EXPORT nimcp_speech_gpu_config_t nimcp_speech_gpu_default_config(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create GPU speech processing state
 *
 * WHAT: Initialize GPU resources for speech feature extraction
 * WHY:  Pre-allocate FFT plans, filterbanks, and buffers
 * HOW:  Create cuFFT plans, compute mel filterbank, allocate GPU memory
 *
 * @param ctx GPU context (required)
 * @param sample_rate Audio sample rate in Hz
 * @param frame_size Frame size in samples
 * @param hop_size Hop size in samples
 * @param num_mel_bins Number of mel filterbank bins
 * @param lpc_order LPC order for formant extraction
 * @return Speech GPU state or NULL on failure
 */
NIMCP_EXPORT nimcp_speech_gpu_state_t* nimcp_speech_gpu_create(
    nimcp_gpu_context_t* ctx,
    int sample_rate,
    int frame_size,
    int hop_size,
    int num_mel_bins,
    int lpc_order
);

/**
 * @brief Create GPU speech processing state with full configuration
 *
 * @param ctx GPU context (required)
 * @param config Full configuration structure
 * @return Speech GPU state or NULL on failure
 */
NIMCP_EXPORT nimcp_speech_gpu_state_t* nimcp_speech_gpu_create_with_config(
    nimcp_gpu_context_t* ctx,
    const nimcp_speech_gpu_config_t* config
);

/**
 * @brief Destroy GPU speech processing state
 *
 * @param state Speech GPU state to destroy
 */
NIMCP_EXPORT void nimcp_speech_gpu_destroy(nimcp_speech_gpu_state_t* state);

/**
 * @brief Synchronize GPU operations
 *
 * @param state Speech GPU state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_speech_gpu_synchronize(nimcp_speech_gpu_state_t* state);

//=============================================================================
// Feature Extraction API
//=============================================================================

/**
 * @brief Extract all speech features from audio (GPU-accelerated)
 *
 * WHAT: Comprehensive feature extraction pipeline
 * WHY:  Single call for all speech features
 * HOW:  Runs STFT -> mel -> MFCC + pitch + formants in parallel
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor [batch, samples] or [samples]
 * @return Feature structure or NULL on failure
 */
NIMCP_EXPORT nimcp_speech_features_gpu_t* nimcp_speech_gpu_extract_features(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Free speech features structure
 *
 * @param features Features to free
 */
NIMCP_EXPORT void nimcp_speech_gpu_free_features(nimcp_speech_features_gpu_t* features);

/**
 * @brief Compute spectrogram (magnitude STFT)
 *
 * WHAT: Short-Time Fourier Transform magnitude
 * WHY:  Foundation for mel spectrogram and other features
 * HOW:  Window -> FFT -> magnitude
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return Spectrogram tensor [num_frames, fft_bins] or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_spectrogram(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Compute mel spectrogram
 *
 * WHAT: Mel-scaled magnitude spectrogram
 * WHY:  Perceptually-weighted frequency representation
 * HOW:  Spectrogram -> mel filterbank matrix multiplication
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return Mel spectrogram tensor [num_frames, num_mel_bins]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mel_spectrogram(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Compute MFCC (Mel-Frequency Cepstral Coefficients)
 *
 * WHAT: Standard speech features for recognition
 * WHY:  Compact, robust representation of spectral envelope
 * HOW:  Mel spectrogram -> log -> DCT
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return MFCC tensor [num_frames, num_mfcc]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mfcc(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Compute MFCC with delta and delta-delta features
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @param include_delta Include delta features
 * @param include_delta2 Include delta-delta features
 * @return MFCC tensor [num_frames, num_mfcc * (1+delta+delta2)]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mfcc_full(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    bool include_delta,
    bool include_delta2
);

//=============================================================================
// Formant Extraction API
//=============================================================================

/**
 * @brief Extract formant frequencies using LPC analysis
 *
 * WHAT: Vocal tract resonance frequencies (F1, F2, F3, F4)
 * WHY:  Essential for vowel recognition and speaker characterization
 * HOW:  LPC coefficients -> polynomial roots -> formant frequencies
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return Formant tensor [num_frames, 4] with F1-F4 frequencies
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_extract_formants(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Extract formants with bandwidths
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @param formants Output formant frequencies [num_frames, 4]
 * @param bandwidths Output formant bandwidths [num_frames, 4]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_speech_gpu_extract_formants_full(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t** formants,
    nimcp_gpu_tensor_t** bandwidths
);

/**
 * @brief Compute LPC coefficients
 *
 * WHAT: Linear Predictive Coding coefficients
 * WHY:  Model vocal tract as all-pole filter
 * HOW:  Autocorrelation -> Levinson-Durbin recursion
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return LPC coefficients [num_frames, lpc_order]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_lpc(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

//=============================================================================
// Pitch Detection API
//=============================================================================

/**
 * @brief Detect pitch (fundamental frequency) using autocorrelation
 *
 * WHAT: Fundamental frequency (F0) estimation
 * WHY:  Essential for prosody, speaker ID, and tone recognition
 * HOW:  Autocorrelation peak detection with parabolic interpolation
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return Pitch tensor [num_frames] with F0 in Hz (0 = unvoiced)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_pitch(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Detect pitch with confidence scores
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @param pitch Output pitch [num_frames]
 * @param confidence Output confidence [num_frames]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_speech_gpu_detect_pitch_full(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t** pitch,
    nimcp_gpu_tensor_t** confidence
);

/**
 * @brief Detect pitch using harmonic product spectrum
 *
 * WHAT: Alternative pitch detection method
 * WHY:  More robust to noise than autocorrelation
 * HOW:  Multiply downsampled spectra to find F0
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @param num_harmonics Number of harmonics to use (default 5)
 * @return Pitch tensor [num_frames]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_pitch_hps(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    int num_harmonics
);

//=============================================================================
// Voice Activity Detection API
//=============================================================================

/**
 * @brief Detect voice activity (VAD)
 *
 * WHAT: Binary speech/non-speech decision per frame
 * WHY:  Filter out silence for efficient processing
 * HOW:  Energy + ZCR thresholding with smoothing
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @param energy_threshold Energy threshold (dB, default -40)
 * @return VAD tensor [num_frames] with 0/1 values
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_vad(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    float energy_threshold
);

/**
 * @brief Compute frame energy in dB
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return Energy tensor [num_frames]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_energy(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Compute zero crossing rate
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return ZCR tensor [num_frames]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_zcr(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

//=============================================================================
// Phoneme Recognition API
//=============================================================================

/**
 * @brief Full speech recognition pipeline (audio -> phonemes)
 *
 * WHAT: End-to-end phoneme recognition
 * WHY:  Complete pipeline for speech understanding
 * HOW:  Features -> neural classifier -> phoneme sequence
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @return Phoneme result or NULL on failure
 */
NIMCP_EXPORT nimcp_phoneme_result_gpu_t* nimcp_speech_gpu_recognize(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
);

/**
 * @brief Free phoneme recognition result
 *
 * @param result Result to free
 */
NIMCP_EXPORT void nimcp_speech_gpu_free_phoneme_result(nimcp_phoneme_result_gpu_t* result);

/**
 * @brief Load phoneme classifier weights
 *
 * WHAT: Load neural network weights for phoneme classification
 * WHY:  Enable phoneme recognition from features
 * HOW:  Load weight matrices for linear classifier layers
 *
 * @param state Speech GPU state
 * @param weights Classifier weights [feature_dim, num_phonemes]
 * @param bias Classifier bias [num_phonemes]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_speech_gpu_load_phoneme_classifier(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* bias
);

/**
 * @brief Classify phonemes from features
 *
 * @param state Speech GPU state
 * @param features Input features [num_frames, feature_dim]
 * @return Phoneme probabilities [num_frames, num_phonemes]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_classify_phonemes(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* features
);

//=============================================================================
// Delta Feature Computation
//=============================================================================

/**
 * @brief Compute delta (velocity) features
 *
 * WHAT: First-order temporal derivatives
 * WHY:  Capture dynamic spectral changes
 * HOW:  Regression over context window
 *
 * @param state Speech GPU state
 * @param features Input features [num_frames, feature_dim]
 * @param context Delta context size (default 2)
 * @return Delta features [num_frames, feature_dim]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_delta(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* features,
    int context
);

/**
 * @brief Compute delta-delta (acceleration) features
 *
 * @param state Speech GPU state
 * @param features Input features
 * @param context Context size
 * @return Delta-delta features
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_delta2(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* features,
    int context
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Apply pre-emphasis filter
 *
 * @param state Speech GPU state
 * @param audio Input audio tensor
 * @param coeff Pre-emphasis coefficient (default 0.97)
 * @return Pre-emphasized audio
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_speech_gpu_preemphasis(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    float coeff
);

/**
 * @brief Apply cepstral mean normalization (CMN)
 *
 * @param state Speech GPU state
 * @param features Input features (modified in-place)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_speech_gpu_apply_cmn(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* features
);

/**
 * @brief Get number of output frames for given audio length
 *
 * @param state Speech GPU state
 * @param num_samples Number of audio samples
 * @return Number of output frames
 */
NIMCP_EXPORT int nimcp_speech_gpu_get_num_frames(
    const nimcp_speech_gpu_state_t* state,
    int num_samples
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Speech GPU processing statistics
 */
typedef struct nimcp_speech_gpu_stats {
    uint64_t frames_processed;          /**< Total frames processed */
    uint64_t fft_operations;            /**< Total FFT operations */
    uint64_t mfcc_extractions;          /**< MFCC extraction count */
    uint64_t pitch_detections;          /**< Pitch detection count */
    uint64_t formant_extractions;       /**< Formant extraction count */
    uint64_t phoneme_classifications;   /**< Phoneme classification count */
    float avg_fft_time_us;              /**< Average FFT time (microseconds) */
    float avg_mfcc_time_us;             /**< Average MFCC time */
    float avg_lpc_time_us;              /**< Average LPC time */
    float avg_pitch_time_us;            /**< Average pitch detection time */
    size_t gpu_memory_used;             /**< GPU memory in use */
} nimcp_speech_gpu_stats_t;

/**
 * @brief Get speech GPU statistics
 *
 * @param state Speech GPU state
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_speech_gpu_get_stats(
    const nimcp_speech_gpu_state_t* state,
    nimcp_speech_gpu_stats_t* stats
);

/**
 * @brief Reset speech GPU statistics
 *
 * @param state Speech GPU state
 */
NIMCP_EXPORT void nimcp_speech_gpu_reset_stats(nimcp_speech_gpu_state_t* state);

//=============================================================================
// CPU Fallback Reference Implementations
//=============================================================================

/**
 * @brief CPU reference: Compute mel filterbank (for testing)
 */
NIMCP_EXPORT bool nimcp_speech_cpu_mel_filterbank(
    const float* power_spectrum,
    int fft_bins,
    const float* filterbank,
    int num_mel_bins,
    float* mel_energies
);

/**
 * @brief CPU reference: Compute autocorrelation (for testing)
 */
NIMCP_EXPORT bool nimcp_speech_cpu_autocorrelation(
    const float* signal,
    int signal_len,
    float* autocorr,
    int max_lag
);

/**
 * @brief CPU reference: Levinson-Durbin recursion (for testing)
 */
NIMCP_EXPORT bool nimcp_speech_cpu_levinson_durbin(
    const float* autocorr,
    int order,
    float* lpc_coeffs,
    float* reflection_coeffs
);

/**
 * @brief CPU reference: DCT for MFCC (for testing)
 */
NIMCP_EXPORT bool nimcp_speech_cpu_dct(
    const float* log_mel,
    int num_mel_bins,
    int num_mfcc,
    float* mfcc
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPEECH_CORTEX_GPU_H */
