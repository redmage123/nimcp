/**
 * @file nimcp_audio_dsp_bridge.h
 * @brief Audio DSP Bridge — proper spectral analysis for audio cortex pipeline
 *
 * WHAT: Inserts DSP engine between raw audio and brain's audio cortex CNN.
 *       Replaces crude power-in-bins mel approximation with proper windowed
 *       STFT, mel filterbank, and optional MFCC extraction.
 * WHY:  The brain's audio CNN expects 128 mel features. The current path
 *       uses a simplistic energy-per-bin calculation that loses temporal
 *       and spectral detail. Proper DSP gives the CNN real spectral content.
 * HOW:  Raw samples → Hamming window → FFT → |X|² → mel filterbank → log
 *       → optional DCT for MFCC. Output always 128 floats for CNN compat.
 *
 * INTEGRATION POINTS:
 *   Training:  raw ESC-50 audio → audio_dsp_process() → submit_sensory("audio")
 *   Inference: sim_perception_bridge → audio_dsp_process() → staged_sensory
 *   Both:      cortex_cnn_forward_audio receives processed 128-dim features
 */

#ifndef NIMCP_AUDIO_DSP_BRIDGE_H
#define NIMCP_AUDIO_DSP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct dsp_engine;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef enum {
    AUDIO_DSP_OUTPUT_MEL_POWER  = 0,  /* log mel filterbank energies (default) */
    AUDIO_DSP_OUTPUT_MFCC       = 1,  /* mel-frequency cepstral coefficients */
    AUDIO_DSP_OUTPUT_MEL_RAW    = 2,  /* linear mel filterbank (no log) */
} audio_dsp_output_type_t;

typedef struct {
    uint32_t    fft_size;           /* FFT window size (default: 512) */
    uint32_t    hop_size;           /* STFT hop (default: 256) */
    uint32_t    num_mel_bins;       /* output dimension (default: 128, must match CNN) */
    uint32_t    num_mfcc;           /* MFCC coefficients (default: 13, if output=MFCC) */
    double      sample_rate;        /* Hz (default: 16000) */
    audio_dsp_output_type_t output_type;
    bool        pre_emphasis;       /* apply pre-emphasis filter (default: true) */
    double      pre_emphasis_coeff; /* typically 0.97 */
    bool        normalize;          /* normalize output to [0,1] (default: true) */
} audio_dsp_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    frames_processed;
    uint64_t    samples_processed;
    double      peak_frequency;     /* dominant frequency in last frame */
    double      spectral_centroid;  /* brightness measure */
    double      spectral_rolloff;   /* frequency below which 85% of energy */
    double      rms_level;          /* average power level */
} audio_dsp_stats_t;

/* ============================================================================
 * Bridge
 * ============================================================================ */

typedef struct audio_dsp_bridge {
    struct dsp_engine*  dsp;        /* DSP engine (owned) */
    audio_dsp_config_t  config;
    audio_dsp_stats_t   stats;
    /* Scratch buffers */
    double*     frame_buffer;       /* [fft_size] windowed frame */
    double*     power_spectrum;     /* [fft_size/2+1] */
    double*     mel_energies;       /* [num_mel_bins] */
    float*      output_buffer;      /* [num_mel_bins] float32 for brain */
    bool        initialized;
} audio_dsp_bridge_t;

/* ============================================================================
 * API
 * ============================================================================ */

/** Create audio DSP bridge */
audio_dsp_bridge_t* audio_dsp_bridge_create(const audio_dsp_config_t* config);

/** Destroy bridge */
void audio_dsp_bridge_destroy(audio_dsp_bridge_t* bridge);

/**
 * @brief Process raw audio samples into mel features for the brain.
 *
 * Takes raw float32 audio samples, applies DSP pipeline (pre-emphasis,
 * windowing, FFT, mel filterbank, log/MFCC), and produces exactly
 * num_mel_bins (128) float32 features ready for cortex_cnn_forward_audio.
 *
 * If the input is longer than one FFT frame, averages across frames
 * (Welch-style) to produce a single feature vector.
 *
 * @param bridge  Audio DSP bridge
 * @param samples Raw audio samples (float32, [-1,1] range)
 * @param num_samples Number of samples
 * @param out_features Output: exactly num_mel_bins float32 features
 * @return 0 on success, -1 on error
 */
int audio_dsp_process(audio_dsp_bridge_t* bridge,
                        const float* samples, uint32_t num_samples,
                        float* out_features);

/**
 * @brief Process and submit to brain in one call.
 *
 * Convenience: processes raw audio then calls brain submit_sensory.
 *
 * @param bridge  Audio DSP bridge
 * @param brain   Brain handle (nimcp_brain_t)
 * @param samples Raw audio samples
 * @param num_samples Number of samples
 * @return 0 on success
 */
struct nimcp_brain_handle;
int audio_dsp_process_and_submit(audio_dsp_bridge_t* bridge,
                                   struct nimcp_brain_handle* brain,
                                   const float* samples, uint32_t num_samples);

/** Get spectral analysis stats from last processed frame */
audio_dsp_stats_t audio_dsp_get_stats(const audio_dsp_bridge_t* bridge);

/** Default config (matches brain expectations: 128 mel bins, 16kHz) */
audio_dsp_config_t audio_dsp_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIO_DSP_BRIDGE_H */
