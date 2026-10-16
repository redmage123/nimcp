/**
 * @file nimcp_vae_auditory_bridge.h
 * @brief Bridge between VAE and Auditory Cortex for Audio Processing
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE with audio cortex for auditory representation learning
 *
 * WHY:  VAE enables:
 *       - Mel/MFCC feature encoding in latent space
 *       - Audio generation from latent codes (imagination)
 *       - Novelty detection for auditory anomalies
 *       - Tonotopic organization in latent dimensions
 *       - Cross-modal binding with visual representations
 *
 * HOW:  Bridge maps audio features to VAE operations:
 *       - FFT → Mel filterbank → VAE encoder → latent
 *       - Latent → VAE decoder → spectral features → audio
 *       - Latent novelty → auditory attention
 *
 * SIGNAL PROCESSING:
 * ==================
 * - FFT: Time-frequency analysis
 * - Mel filterbank: Perceptual frequency scaling
 * - MFCC: Cepstral coefficients for speech/audio
 * - Hilbert: Amplitude envelope extraction
 *
 * BIO_MODULE: 0x1F17 (VAE-Auditory Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_AUDITORY_BRIDGE_H
#define NIMCP_VAE_AUDITORY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VAE_AUDITORY_BRIDGE_VERSION     "1.0.0"
#define BIO_MODULE_VAE_AUDITORY_BRIDGE  0x1F17

#define VAE_AUDIO_DEFAULT_SAMPLE_RATE   16000
#define VAE_AUDIO_DEFAULT_FFT_SIZE      512
#define VAE_AUDIO_DEFAULT_MEL_BINS      80
#define VAE_AUDIO_DEFAULT_MFCC_COEFFS   13

/** Error code range (32480-32489) */
#define NIMCP_ERROR_VAE_AUDIO_BASE          32480
#define NIMCP_ERROR_VAE_AUDIO_NULL          32481
#define NIMCP_ERROR_VAE_AUDIO_NOT_CONNECTED 32482
#define NIMCP_ERROR_VAE_AUDIO_ENCODE_FAILED 32483
#define NIMCP_ERROR_VAE_AUDIO_DECODE_FAILED 32484
#define NIMCP_ERROR_VAE_AUDIO_NO_MEMORY     32485
#define NIMCP_ERROR_VAE_AUDIO_FFT_FAILED    32486

/* ============================================================================
 * Enumerations
 * ============================================================================ */

typedef enum {
    VAE_AUDIO_MODE_ENCODE = 0,
    VAE_AUDIO_MODE_DECODE,
    VAE_AUDIO_MODE_RECONSTRUCT,
    VAE_AUDIO_MODE_GENERATE
} vae_audio_mode_t;

typedef enum {
    VAE_AUDIO_FEAT_MEL = 0,          /**< Mel spectrogram */
    VAE_AUDIO_FEAT_MFCC,              /**< MFCC coefficients */
    VAE_AUDIO_FEAT_RAW_SPECTRUM,      /**< Raw FFT spectrum */
    VAE_AUDIO_FEAT_ENVELOPE           /**< Amplitude envelope */
} vae_audio_feature_t;

typedef enum {
    VAE_AUDIO_STATE_DISCONNECTED = 0,
    VAE_AUDIO_STATE_CONNECTED,
    VAE_AUDIO_STATE_ENCODING,
    VAE_AUDIO_STATE_DECODING,
    VAE_AUDIO_STATE_ERROR
} vae_audio_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

typedef struct {
    uint32_t sample_rate;
    uint32_t fft_size;
    uint32_t hop_size;
    uint32_t mel_bins;
    uint32_t mfcc_coeffs;
    float min_freq;
    float max_freq;

    vae_audio_feature_t feature_type;
    bool enable_delta_features;       /**< First/second derivative */
    bool enable_energy;               /**< Log energy feature */

    bool enable_novelty_detection;
    float novelty_threshold;

    bool enable_logging;
} vae_audio_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

typedef struct {
    float* latent;
    uint32_t latent_dim;
    float* features;                  /**< Extracted features (mel/mfcc) */
    uint32_t feature_dim;
    float novelty_score;
    float energy;
    uint64_t encoding_time_us;
} vae_audio_encode_result_t;

typedef struct {
    float* audio;                     /**< Reconstructed audio samples */
    uint32_t num_samples;
    float* spectrum;                  /**< Reconstructed spectrum */
    uint32_t spectrum_dim;
    float reconstruction_error;
    uint64_t decoding_time_us;
} vae_audio_decode_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_encodes;
    uint64_t total_decodes;
    uint64_t frames_processed;
    float avg_novelty_score;
    float avg_reconstruction_error;
    float avg_encoding_latency_us;
    uint64_t creation_time_us;
    uint64_t last_operation_us;
} vae_audio_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_audio_bridge {
    vae_audio_bridge_config_t config;
    vae_system_t* vae;
    void* audio_cortex;
    vae_audio_bridge_state_t state;
    bool is_initialized;

    /* Working buffers */
    float* fft_buffer;
    float* mel_buffer;
    float* mfcc_buffer;
    float* window;

    /* FFT state */
    void* fft_plan;

    /* Novelty baseline */
    float* novelty_baseline;
    uint64_t baseline_samples;

    /* Statistics */
    vae_audio_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_audio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_audio_bridge_default_config(vae_audio_bridge_config_t* config);
vae_audio_bridge_t* vae_audio_bridge_create(const vae_audio_bridge_config_t* config);
void vae_audio_bridge_destroy(vae_audio_bridge_t* bridge);
int vae_audio_bridge_connect_vae(vae_audio_bridge_t* bridge, vae_system_t* vae);
int vae_audio_bridge_connect_cortex(vae_audio_bridge_t* bridge, void* audio_cortex);
int vae_audio_bridge_disconnect(vae_audio_bridge_t* bridge);
bool vae_audio_bridge_is_connected(const vae_audio_bridge_t* bridge);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int vae_audio_encode(vae_audio_bridge_t* bridge,
                      const float* audio, uint32_t num_samples,
                      vae_audio_encode_result_t* result);

int vae_audio_encode_spectrum(vae_audio_bridge_t* bridge,
                               const float* spectrum, uint32_t spectrum_dim,
                               vae_audio_encode_result_t* result);

int vae_audio_encode_features(vae_audio_bridge_t* bridge,
                               const float* features, uint32_t feature_dim,
                               vae_audio_encode_result_t* result);

/* ============================================================================
 * Decoding API
 * ============================================================================ */

int vae_audio_decode(vae_audio_bridge_t* bridge,
                      const float* latent, uint32_t latent_dim,
                      vae_audio_decode_result_t* result);

int vae_audio_generate(vae_audio_bridge_t* bridge,
                        float temperature,
                        vae_audio_decode_result_t* result);

/* ============================================================================
 * Feature Extraction API
 * ============================================================================ */

int vae_audio_compute_mel(vae_audio_bridge_t* bridge,
                           const float* audio, uint32_t num_samples,
                           float* mel_features, uint32_t* mel_dim);

int vae_audio_compute_mfcc(vae_audio_bridge_t* bridge,
                            const float* audio, uint32_t num_samples,
                            float* mfcc_features, uint32_t* mfcc_dim);

int vae_audio_compute_novelty(vae_audio_bridge_t* bridge,
                               const float* audio, uint32_t num_samples,
                               float* novelty_score);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_audio_bridge_state_t vae_audio_bridge_get_state(const vae_audio_bridge_t* bridge);
int vae_audio_bridge_get_stats(const vae_audio_bridge_t* bridge,
                                vae_audio_bridge_stats_t* stats);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_audio_encode_result_free(vae_audio_encode_result_t* result);
void vae_audio_decode_result_free(vae_audio_decode_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_AUDITORY_BRIDGE_H */
