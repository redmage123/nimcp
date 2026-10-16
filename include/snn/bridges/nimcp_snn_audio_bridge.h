/**
 * @file nimcp_snn_audio_bridge.h
 * @brief SNN integration bridge for Audio Cortex
 *
 * WHAT: Bidirectional bridge between SNN and audio_cortex_t
 * WHY:  Enable spike-based auditory processing with temporal spike patterns
 * WHY:  Encode spectrograms to spike trains, decode SNN output to audio features
 * HOW:  Rate coding for spectrograms, temporal coding for audio events
 *
 * BIOLOGICAL BASIS:
 * - Auditory cortex (A1) exhibits precise spike timing for sound processing
 * - Cochlear neurons encode frequency via tonotopic spike patterns
 * - Temporal spike patterns critical for speech and music perception
 * - Phase locking in auditory neurons encodes frequency information
 *
 * INTEGRATION PATTERN:
 * - Audio spectrograms → SNN encoding → Spike trains
 * - Temporal patterns → Spike timing → Audio events (onset/offset)
 * - SNN output spikes → Decoding → Audio features/MFCC
 * - Bio-async for auditory event messaging
 *
 * NIMCP STANDARDS:
 * - WHAT-WHY-HOW documentation
 * - Guard clauses (early returns)
 * - Functions < 50 lines
 * - Bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_SNN_AUDIO_BRIDGE_H
#define NIMCP_SNN_AUDIO_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_encoding.h"
#include "perception/nimcp_audio_cortex.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Audio-SNN bridge configuration
 *
 * WHAT: Parameters for audio cortex to SNN integration
 * WHY:  Control encoding/decoding for temporal audio processing
 * HOW:  Thresholds, encoding method, spectrotemporal parameters
 */
typedef struct snn_audio_config_s {
    /* Encoding configuration */
    snn_encoding_t encoding_method;     /**< Encoding type (rate/temporal) */
    float max_spike_rate;               /**< Maximum firing rate (Hz) */
    float min_spike_rate;               /**< Baseline firing rate (Hz) */
    float temporal_window_ms;           /**< Spike integration window (ms) */
    uint32_t neurons_per_freq_bin;      /**< Neurons per frequency bin */

    /* Decoding configuration */
    snn_decoding_t decoding_method;     /**< Decoding type (rate/temporal) */
    float decode_window_ms;             /**< Decoding time window (ms) */
    bool use_first_spike;               /**< Use first-spike decoding */

    /* Audio processing */
    uint32_t sample_rate;               /**< Audio sample rate (Hz) */
    uint32_t frame_size;                /**< Audio frame size (samples) */
    uint32_t num_freq_bins;             /**< Number of frequency bins */
    uint32_t num_mel_filters;           /**< Number of mel filters */
    bool encode_mfcc;                   /**< Encode MFCC instead of spectrum */

    /* Temporal coding */
    bool use_onset_detection;           /**< Encode onset events */
    bool use_phase_locking;             /**< Use phase-locked spikes */
    float phase_lock_freq_max;          /**< Max frequency for phase locking (Hz) */

    /* Attention modulation */
    bool use_attention_modulation;      /**< Modulate by audio attention */
    float attention_gain_min;           /**< Min attention gain */
    float attention_gain_max;           /**< Max attention gain */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    float update_interval_ms;           /**< Update interval (ms) */

} snn_audio_config_t;

/**
 * @brief Audio encoding statistics
 *
 * WHAT: Metrics for audio-to-spike conversion
 * WHY:  Monitor encoding efficiency
 * HOW:  Track spikes, rates, temporal patterns
 */
typedef struct snn_audio_encode_stats_s {
    uint64_t frames_encoded;            /**< Total frames encoded */
    uint64_t total_spikes;              /**< Total spikes generated */
    float avg_spikes_per_frame;         /**< Average spikes per frame */
    float avg_encoding_time_ms;         /**< Average encoding time (ms) */
    float peak_spike_rate;              /**< Peak firing rate (Hz) */
    float mean_spike_rate;              /**< Mean firing rate (Hz) */
    uint32_t onset_events_detected;     /**< Number of onset events */
    uint32_t offset_events_detected;    /**< Number of offset events */
} snn_audio_encode_stats_t;

/**
 * @brief Audio decoding statistics
 *
 * WHAT: Metrics for spike-to-audio conversion
 * WHY:  Monitor decoding quality
 * HOW:  Track decoded frames and reconstruction
 */
typedef struct snn_audio_decode_stats_s {
    uint64_t frames_decoded;            /**< Total frames decoded */
    float avg_decoding_time_ms;         /**< Average decoding time (ms) */
    float reconstruction_error;         /**< MSE of reconstruction */
    uint32_t silent_bins;               /**< Frequency bins with no spikes */
} snn_audio_decode_stats_t;

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Audio-SNN bridge structure
 *
 * WHAT: Context for audio cortex to SNN integration
 * WHY:  Maintain state for bidirectional audio-spike flow
 * HOW:  Store SNN, audio cortex, encoders/decoders, buffers
 */
typedef struct snn_audio_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /* Connected systems */
    snn_network_t* snn;                 /**< SNN network */
    audio_cortex_t* audio_cortex;       /**< Audio cortex module */

    /* Configuration */
    snn_audio_config_t config;          /**< Bridge configuration */

    /* Encoding/Decoding */
    snn_encoder_t* encoder;             /**< Audio-to-spike encoder */
    snn_decoder_t* decoder;             /**< Spike-to-audio decoder */

    /* Working buffers */
    float* audio_buffer;                /**< Input audio buffer */
    float* spectrum_buffer;             /**< FFT spectrum buffer */
    float* mel_buffer;                  /**< Mel-scale features */
    float* mfcc_buffer;                 /**< MFCC coefficients */
    float* spike_input_buffer;          /**< Normalized feature values */
    float* spike_output_buffer;         /**< Decoded output values */
    uint8_t* spike_mask;                /**< Binary spike mask */

    /* Temporal pattern detection */
    float* onset_strength;              /**< Onset strength per bin */
    float* prev_spectrum;               /**< Previous spectrum for diff */
    bool* onset_detected;               /**< Onset flags per bin */

    /* Attention integration */
    audio_attention_map_t* attention_map; /**< Attention from audio cortex */
    float* attention_gains;             /**< Per-neuron attention gains */

    /* Statistics */
    snn_audio_encode_stats_t encode_stats; /**< Encoding statistics */
    snn_audio_decode_stats_t decode_stats; /**< Decoding statistics */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time_ms;          /**< Last update timestamp */
    uint32_t frame_count;               /**< Frame counter */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Thread safety */
    void* mutex;                        /**< Mutex for thread safety */

} snn_audio_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize audio bridge config with defaults
 *
 * WHAT: Set sensible default parameters
 * WHY:  Convenient initialization
 * HOW:  Values for typical audio processing
 *
 * DEFAULTS:
 * - Rate coding at 50-200 Hz
 * - 16kHz sample rate, 512 sample frames
 * - 128 mel filters, 13 MFCC coefficients
 * - Onset detection enabled
 *
 * @param config Config to initialize
 */
void snn_audio_config_default(snn_audio_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create audio-SNN bridge
 *
 * WHAT: Initialize bidirectional bridge between audio cortex and SNN
 * WHY:  Enable spike-based auditory processing
 * HOW:  Allocate buffers, create encoder/decoder, set up connections
 *
 * @param config Bridge configuration
 * @param snn SNN network to connect
 * @param audio_cortex Audio cortex module to connect
 * @return Bridge instance or NULL on failure
 */
snn_audio_bridge_t* snn_audio_bridge_create(
    const snn_audio_config_t* config,
    snn_network_t* snn,
    audio_cortex_t* audio_cortex
);

/**
 * @brief Destroy audio-SNN bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper memory management
 * HOW:  Free buffers, destroy encoder/decoder, disconnect
 *
 * @param bridge Bridge to destroy
 */
void snn_audio_bridge_destroy(snn_audio_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging for audio events
 * WHY:  Distributed spike event coordination
 * HOW:  Register with bio-router as BIO_MODULE_SNN_AUDIO
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_audio_bridge_connect_bio_async(snn_audio_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_audio_bridge_disconnect_bio_async(snn_audio_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_audio_bridge_is_bio_async_connected(const snn_audio_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode audio spectrogram to spike trains
 *
 * WHAT: Convert audio spectrum to spike trains
 * WHY:  Enable SNN processing of auditory input
 * HOW:  Rate/temporal encoding with tonotopic organization
 *
 * ALGORITHM:
 * 1. Compute FFT/mel-scale spectrum
 * 2. Detect onsets if enabled
 * 3. Normalize frequency bins [0, 1]
 * 4. Apply attention modulation if enabled
 * 5. Encode to spike trains (tonotopic mapping)
 * 6. Feed spikes to SNN input layer
 *
 * @param bridge Audio-SNN bridge
 * @param audio_data Input audio samples
 * @param num_samples Number of samples
 * @param num_channels Number of audio channels (1 or 2)
 * @param spike_trains Output spike trains
 * @return 0 on success, error code on failure
 */
int snn_audio_bridge_encode(
    snn_audio_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    uint8_t num_channels,
    snn_spike_train_t** spike_trains
);

/**
 * @brief Encode audio features (MFCC) to spikes
 *
 * WHAT: Convert MFCC features to spike trains
 * WHY:  Enable SNN processing of high-level audio features
 * HOW:  Population coding of MFCC coefficients
 *
 * @param bridge Audio-SNN bridge
 * @param features MFCC feature vector
 * @param num_features Feature dimension
 * @param spike_trains Output spike trains
 * @return 0 on success, error code on failure
 */
int snn_audio_bridge_encode_features(
    snn_audio_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    snn_spike_train_t** spike_trains
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Decode spike trains to audio spectrogram
 *
 * WHAT: Convert SNN output spikes to audio spectrum
 * WHY:  Reconstruct audio from spike activity
 * HOW:  Rate/first-spike decoding to frequency bins
 *
 * @param bridge Audio-SNN bridge
 * @param spike_trains Input spike trains
 * @param num_trains Number of spike trains
 * @param spectrum_out Output spectrum [num_freq_bins]
 * @return 0 on success, error code on failure
 */
int snn_audio_bridge_decode(
    snn_audio_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    float* spectrum_out
);

/**
 * @brief Decode spike trains to audio features
 *
 * WHAT: Convert SNN output spikes to MFCC features
 * WHY:  Extract high-level audio features from SNN
 * HOW:  Population vector decoding
 *
 * @param bridge Audio-SNN bridge
 * @param spike_trains Input spike trains
 * @param num_trains Number of spike trains
 * @param features_out Output feature vector
 * @param num_features Feature dimension
 * @return 0 on success, error code on failure
 */
int snn_audio_bridge_decode_features(
    snn_audio_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    float* features_out,
    uint32_t num_features
);

//=============================================================================
// Temporal Pattern Functions
//=============================================================================

/**
 * @brief Detect audio onset events from spike patterns
 *
 * WHAT: Identify sound onset from rapid spike rate increase
 * WHY:  Temporal events critical for audio perception
 * HOW:  Compare current and previous spike rates
 *
 * @param bridge Audio-SNN bridge
 * @param onset_detected Output onset flags [num_freq_bins]
 * @return Number of onsets detected
 */
uint32_t snn_audio_bridge_detect_onsets(
    snn_audio_bridge_t* bridge,
    bool* onset_detected
);

/**
 * @brief Encode temporal audio pattern to spike timing
 *
 * WHAT: Convert audio envelope to precise spike times
 * WHY:  Temporal coding for rapid audio processing
 * HOW:  Latency coding based on audio intensity
 *
 * @param bridge Audio-SNN bridge
 * @param envelope Audio envelope
 * @param num_samples Envelope length
 * @param spike_times_out Output spike times
 * @return 0 on success, error code on failure
 */
int snn_audio_bridge_encode_temporal(
    snn_audio_bridge_t* bridge,
    const float* envelope,
    uint32_t num_samples,
    float* spike_times_out
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update bridge state and process audio input
 *
 * WHAT: Full update cycle for audio-SNN bridge
 * WHY:  Single call for regular processing
 * HOW:  Encode audio, update SNN, decode output
 *
 * @param bridge Bridge to update
 * @param dt Simulation timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_audio_bridge_update(snn_audio_bridge_t* bridge, float dt);

/**
 * @brief Update attention modulation from audio cortex
 *
 * WHAT: Get attention map and compute per-frequency gains
 * WHY:  Modulate spike rates by auditory salience
 * HOW:  Sample attention map, scale encoding rates
 *
 * @param bridge Bridge to update
 * @return 0 on success, error code on failure
 */
int snn_audio_bridge_update_attention(snn_audio_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get encoding statistics
 *
 * @param bridge Bridge to query
 * @param stats Output encoding statistics
 * @return 0 on success
 */
int snn_audio_bridge_get_encode_stats(
    const snn_audio_bridge_t* bridge,
    snn_audio_encode_stats_t* stats
);

/**
 * @brief Get decoding statistics
 *
 * @param bridge Bridge to query
 * @param stats Output decoding statistics
 * @return 0 on success
 */
int snn_audio_bridge_get_decode_stats(
    const snn_audio_bridge_t* bridge,
    snn_audio_decode_stats_t* stats
);

/**
 * @brief Get current spike rate for frequency bin
 *
 * @param bridge Bridge to query
 * @param freq_bin Frequency bin index
 * @return Current spike rate (Hz), -1.0 on error
 */
float snn_audio_bridge_get_spike_rate(
    const snn_audio_bridge_t* bridge,
    uint32_t freq_bin
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge to check
 * @return true if connected and active
 */
bool snn_audio_bridge_is_active(const snn_audio_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_audio_bridge_reset_stats(snn_audio_bridge_t* bridge);

//=============================================================================
// Bio-Async Module ID
//=============================================================================

/** Bio-async module ID for audio-SNN bridge (0x0611) */
#define BIO_MODULE_SNN_AUDIO 0x0611

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_AUDIO_BRIDGE_H */
