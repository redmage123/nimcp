/**
 * @file nimcp_snn_speech_bridge.h
 * @brief SNN integration bridge for Speech Cortex
 *
 * WHAT: Bidirectional bridge between SNN and speech_cortex_t
 * WHY:  Enable spike-based speech processing with phoneme encoding/decoding
 * HOW:  Population coding for phonemes, temporal coding for sequences
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Gyrus (STG) contains phoneme-selective spiking neurons
 * - Speech perception relies on precise spike timing in auditory cortex
 * - Motor cortex (Broca's area) generates spike sequences for articulation
 * - Phonological working memory exhibits spike-based serial order encoding
 *
 * INTEGRATION PATTERN:
 * - Phoneme sequences → SNN encoding → Spike trains
 * - Temporal spike patterns → Phoneme timing → Speech production
 * - SNN output spikes → Decoding → Phoneme recognition
 * - Bio-async for speech event messaging
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

#ifndef NIMCP_SNN_SPEECH_BRIDGE_H
#define NIMCP_SNN_SPEECH_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_encoding.h"
#include "perception/nimcp_speech_cortex.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Speech-SNN bridge configuration
 *
 * WHAT: Parameters for speech cortex to SNN integration
 * WHY:  Control phoneme encoding/decoding and temporal patterns
 * HOW:  Thresholds, encoding method, phonological buffer settings
 */
typedef struct snn_speech_config_s {
    /* Encoding configuration */
    snn_encoding_t encoding_method;     /**< Encoding type (population/temporal) */
    float max_spike_rate;               /**< Maximum firing rate (Hz) */
    float min_spike_rate;               /**< Baseline firing rate (Hz) */
    float temporal_window_ms;           /**< Spike integration window (ms) */
    uint32_t neurons_per_phoneme;       /**< Neurons encoding each phoneme */

    /* Decoding configuration */
    snn_decoding_t decoding_method;     /**< Decoding type (population/first-spike) */
    float decode_window_ms;             /**< Decoding time window (ms) */
    bool use_winner_take_all;           /**< Use WTA for phoneme selection */

    /* Phoneme processing */
    uint32_t num_phonemes;              /**< Phoneme vocabulary size (PHONEME_COUNT) */
    uint32_t num_formants;              /**< Number of formants (4) */
    bool encode_formants;               /**< Encode formant features */
    bool encode_prosody;                /**< Encode pitch/stress */

    /* Temporal coding */
    bool use_sequence_encoding;         /**< Encode phoneme sequences */
    bool use_position_encoding;         /**< Add position in sequence */
    uint32_t max_sequence_length;       /**< Max phoneme sequence length */
    float inter_phoneme_interval_ms;    /**< Time between phonemes */

    /* Phonological working memory */
    bool encode_buffer_position;        /**< Encode buffer position (7±2) */
    uint32_t buffer_capacity;           /**< Working memory capacity */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    float update_interval_ms;           /**< Update interval (ms) */

} snn_speech_config_t;

/**
 * @brief Speech encoding statistics
 *
 * WHAT: Metrics for speech-to-spike conversion
 * WHY:  Monitor encoding efficiency
 * HOW:  Track spikes, phonemes, temporal patterns
 */
typedef struct snn_speech_encode_stats_s {
    uint64_t phonemes_encoded;          /**< Total phonemes encoded */
    uint64_t total_spikes;              /**< Total spikes generated */
    float avg_spikes_per_phoneme;       /**< Average spikes per phoneme */
    float avg_encoding_time_ms;         /**< Average encoding time (ms) */
    float peak_spike_rate;              /**< Peak firing rate (Hz) */
    float mean_spike_rate;              /**< Mean firing rate (Hz) */
    uint32_t sequences_encoded;         /**< Number of sequences */
    float avg_sequence_length;          /**< Average sequence length */
} snn_speech_encode_stats_t;

/**
 * @brief Speech decoding statistics
 *
 * WHAT: Metrics for spike-to-speech conversion
 * WHY:  Monitor decoding accuracy
 * HOW:  Track decoded phonemes and recognition
 */
typedef struct snn_speech_decode_stats_s {
    uint64_t phonemes_decoded;          /**< Total phonemes decoded */
    float avg_decoding_time_ms;         /**< Average decoding time (ms) */
    float phoneme_accuracy;             /**< Recognition accuracy [0, 1] */
    uint32_t confusions;                /**< Phoneme confusions */
    uint32_t silent_outputs;            /**< Outputs with no winner */
} snn_speech_decode_stats_t;

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Speech-SNN bridge structure
 *
 * WHAT: Context for speech cortex to SNN integration
 * WHY:  Maintain state for bidirectional speech-spike flow
 * HOW:  Store SNN, speech cortex, encoders/decoders, buffers
 */
typedef struct snn_speech_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /* Connected systems */
    snn_network_t* snn;                 /**< SNN network */
    speech_cortex_t* speech_cortex;     /**< Speech cortex module */

    /* Configuration */
    snn_speech_config_t config;         /**< Bridge configuration */

    /* Encoding/Decoding */
    snn_encoder_t* encoder;             /**< Speech-to-spike encoder */
    snn_decoder_t* decoder;             /**< Spike-to-speech decoder */

    /* Working buffers */
    float* formant_buffer;              /**< Formant features [num_formants] */
    float* phoneme_features_buffer;     /**< Phoneme feature vector */
    float* spike_input_buffer;          /**< Normalized feature values */
    float* spike_output_buffer;         /**< Decoded output values */
    uint8_t* spike_mask;                /**< Binary spike mask */

    /* Phoneme sequence buffers */
    phoneme_t* phoneme_sequence;        /**< Current phoneme sequence */
    float* sequence_spike_times;        /**< Spike times for sequence */
    uint32_t sequence_length;           /**< Current sequence length */

    /* Phonological buffer encoding */
    phoneme_t* phonological_buffer;     /**< Working memory buffer */
    float* buffer_position_encoding;    /**< Position embeddings */
    uint32_t buffer_fill;               /**< Number of items in buffer */

    /* Phoneme population coding */
    float** phoneme_tuning_curves;      /**< Tuning curves [num_phonemes][neurons_per_phoneme] */
    float* phoneme_preferred_values;    /**< Preferred formant values per neuron */

    /* Statistics */
    snn_speech_encode_stats_t encode_stats; /**< Encoding statistics */
    snn_speech_decode_stats_t decode_stats; /**< Decoding statistics */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time_ms;          /**< Last update timestamp */
    uint32_t phoneme_count;             /**< Phoneme counter */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Thread safety */
    void* mutex;                        /**< Mutex for thread safety */

    /* Language bridge integration (Phase 8.6) */
    struct snn_language_bridge* lang_bridge;  /**< Connected language bridge */
    phoneme_t  phoneme_accum[64];       /**< Phoneme accumulation buffer */
    uint32_t   phoneme_accum_count;     /**< Number of accumulated phonemes */
    float      last_phoneme_time_ms;    /**< Time of last phoneme for gap detection */

} snn_speech_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize speech bridge config with defaults
 *
 * WHAT: Set sensible default parameters
 * WHY:  Convenient initialization
 * HOW:  Values for typical speech processing
 *
 * DEFAULTS:
 * - Population coding with 10 neurons per phoneme
 * - PHONEME_COUNT phonemes (English IPA subset: 38)
 * - 4 formants (F1-F4)
 * - Sequence encoding with position
 * - 9-item phonological buffer (7±2)
 *
 * @param config Config to initialize
 */
void snn_speech_config_default(snn_speech_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create speech-SNN bridge
 *
 * WHAT: Initialize bidirectional bridge between speech cortex and SNN
 * WHY:  Enable spike-based speech processing
 * HOW:  Allocate buffers, create encoder/decoder, initialize tuning curves
 *
 * @param config Bridge configuration
 * @param snn SNN network to connect
 * @param speech_cortex Speech cortex module to connect
 * @return Bridge instance or NULL on failure
 */
snn_speech_bridge_t* snn_speech_bridge_create(
    const snn_speech_config_t* config,
    snn_network_t* snn,
    speech_cortex_t* speech_cortex
);

/**
 * @brief Destroy speech-SNN bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper memory management
 * HOW:  Free buffers, destroy encoder/decoder, disconnect
 *
 * @param bridge Bridge to destroy
 */
void snn_speech_bridge_destroy(snn_speech_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging for speech events
 * WHY:  Distributed spike event coordination
 * HOW:  Register with bio-router as BIO_MODULE_SNN_SPEECH
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_connect_bio_async(snn_speech_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_speech_bridge_disconnect_bio_async(snn_speech_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_speech_bridge_is_bio_async_connected(const snn_speech_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode phoneme to spike trains
 *
 * WHAT: Convert phoneme features to spike trains
 * WHY:  Enable SNN processing of phoneme input
 * HOW:  Population coding with Gaussian tuning curves
 *
 * ALGORITHM:
 * 1. Extract phoneme features (formants, duration, etc.)
 * 2. Compute population activity using tuning curves
 * 3. Convert activity to spike rates
 * 4. Generate spike trains (Poisson or temporal)
 * 5. Feed spikes to SNN input layer
 *
 * @param bridge Speech-SNN bridge
 * @param phoneme Phoneme to encode
 * @param features Phoneme features (formants, prosody)
 * @param spike_trains Output spike trains
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_encode_phoneme(
    snn_speech_bridge_t* bridge,
    phoneme_t phoneme,
    const phoneme_features_t* features,
    snn_spike_train_t** spike_trains
);

/**
 * @brief Encode phoneme sequence to spike patterns
 *
 * WHAT: Convert phoneme sequence to temporal spike patterns
 * WHY:  Enable SNN processing of word/sentence input
 * HOW:  Temporal coding with inter-phoneme intervals
 *
 * ALGORITHM:
 * 1. For each phoneme in sequence:
 *    a. Encode phoneme to population spikes
 *    b. Add position encoding if enabled
 *    c. Offset spike times by inter-phoneme interval
 * 2. Combine into single spike train sequence
 *
 * @param bridge Speech-SNN bridge
 * @param phonemes Phoneme sequence
 * @param num_phonemes Sequence length
 * @param features Array of phoneme features
 * @param spike_trains Output spike trains
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_encode_sequence(
    snn_speech_bridge_t* bridge,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    const phoneme_features_t* features,
    snn_spike_train_t** spike_trains
);

/**
 * @brief Encode phonological buffer to spikes
 *
 * WHAT: Convert working memory buffer to spike representation
 * WHY:  Enable SNN processing of phonological working memory
 * HOW:  Position-dependent encoding of buffer items (7±2)
 *
 * BIOLOGY: Left inferior parietal cortex (BA 40) phonological store
 *
 * @param bridge Speech-SNN bridge
 * @param buffer Phonological buffer (phoneme sequence)
 * @param buffer_size Number of items in buffer
 * @param spike_trains Output spike trains
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_encode_phonological_buffer(
    snn_speech_bridge_t* bridge,
    const phoneme_t* buffer,
    uint32_t buffer_size,
    snn_spike_train_t** spike_trains
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Decode spike trains to phoneme
 *
 * WHAT: Convert SNN output spikes to phoneme classification
 * WHY:  Recognize phoneme from spike activity
 * HOW:  Population vector or winner-take-all decoding
 *
 * @param bridge Speech-SNN bridge
 * @param spike_trains Input spike trains
 * @param num_trains Number of spike trains
 * @param phoneme_out Decoded phoneme
 * @param confidence_out Recognition confidence [0, 1]
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_decode_phoneme(
    snn_speech_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    phoneme_t* phoneme_out,
    float* confidence_out
);

/**
 * @brief Decode spike patterns to phoneme sequence
 *
 * WHAT: Convert temporal spike patterns to phoneme sequence
 * WHY:  Recognize word/sentence from spike activity
 * HOW:  Temporal segmentation + phoneme decoding
 *
 * @param bridge Speech-SNN bridge
 * @param spike_trains Input spike trains
 * @param num_trains Number of spike trains
 * @param phonemes_out Output phoneme sequence
 * @param max_phonemes Maximum phonemes to decode
 * @param num_decoded Number of phonemes decoded
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_decode_sequence(
    snn_speech_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    phoneme_t* phonemes_out,
    uint32_t max_phonemes,
    uint32_t* num_decoded
);

/**
 * @brief Decode spikes to phoneme features
 *
 * WHAT: Convert SNN spikes to phoneme features (formants, etc.)
 * WHY:  Extract acoustic features from spike representation
 * HOW:  Population vector decoding to formant space
 *
 * @param bridge Speech-SNN bridge
 * @param spike_trains Input spike trains
 * @param num_trains Number of spike trains
 * @param features_out Output phoneme features
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_decode_features(
    snn_speech_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    phoneme_features_t* features_out
);

//=============================================================================
// Tuning Curve Functions
//=============================================================================

/**
 * @brief Initialize phoneme tuning curves
 *
 * WHAT: Create Gaussian tuning curves for phoneme population coding
 * WHY:  Model phoneme-selective neurons in STG
 * HOW:  Gaussian receptive fields in formant space (F1, F2)
 *
 * BIOLOGY: STG neurons are tuned to specific phoneme categories
 *
 * @param bridge Speech-SNN bridge
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_init_tuning_curves(snn_speech_bridge_t* bridge);

/**
 * @brief Compute population activity for phoneme features
 *
 * WHAT: Evaluate tuning curves for given phoneme features
 * WHY:  Convert features to population spike rates
 * HOW:  Gaussian activation based on feature distance
 *
 * @param bridge Speech-SNN bridge
 * @param features Phoneme features (formants)
 * @param activities_out Output population activities [num_neurons]
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_compute_population_activity(
    snn_speech_bridge_t* bridge,
    const phoneme_features_t* features,
    float* activities_out
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update bridge state and process speech input
 *
 * WHAT: Full update cycle for speech-SNN bridge
 * WHY:  Single call for regular processing
 * HOW:  Encode phonemes, update SNN, decode output
 *
 * @param bridge Bridge to update
 * @param dt Simulation timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_speech_bridge_update(snn_speech_bridge_t* bridge, float dt);

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
int snn_speech_bridge_get_encode_stats(
    const snn_speech_bridge_t* bridge,
    snn_speech_encode_stats_t* stats
);

/**
 * @brief Get decoding statistics
 *
 * @param bridge Bridge to query
 * @param stats Output decoding statistics
 * @return 0 on success
 */
int snn_speech_bridge_get_decode_stats(
    const snn_speech_bridge_t* bridge,
    snn_speech_decode_stats_t* stats
);

/**
 * @brief Get current spike rate for phoneme
 *
 * @param bridge Bridge to query
 * @param phoneme Phoneme to query
 * @return Current spike rate (Hz), -1.0 on error
 */
float snn_speech_bridge_get_phoneme_spike_rate(
    const snn_speech_bridge_t* bridge,
    phoneme_t phoneme
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge to check
 * @return true if connected and active
 */
bool snn_speech_bridge_is_active(const snn_speech_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_speech_bridge_reset_stats(snn_speech_bridge_t* bridge);

//=============================================================================
// Language Bridge Integration (Phase 8.6)
//=============================================================================

struct snn_language_bridge;

/**
 * @brief Connect speech bridge to SNN language bridge for word-level binding
 *
 * When connected, decoded phoneme sequences are accumulated and on word
 * boundary detection, the corresponding word population is fired on the
 * language bridge (enabling STDP word-concept binding from auditory input).
 */
int snn_speech_bridge_set_language_bridge(
    snn_speech_bridge_t* bridge,
    struct snn_language_bridge* lang_bridge);

/**
 * @brief Feed decoded phonemes through word accumulator
 *
 * Accumulates phonemes until a word boundary is detected (silence gap or
 * known word match), then fires the word population on the connected
 * language bridge.
 *
 * @param bridge Speech bridge
 * @param phoneme Decoded phoneme
 * @param time_ms Current time in milliseconds
 * @return 0 on success, word_pop index if word was fired, -1 on error
 */
int snn_speech_bridge_accumulate_phoneme(
    snn_speech_bridge_t* bridge,
    phoneme_t phoneme,
    float time_ms);

/**
 * @brief Flush phoneme accumulator (force word boundary)
 */
int snn_speech_bridge_flush_accumulator(snn_speech_bridge_t* bridge, float time_ms);

//=============================================================================
// Bio-Async Module ID
//=============================================================================

/** Bio-async module ID for speech-SNN bridge (0x0612) */
#define BIO_MODULE_SNN_SPEECH 0x0612

//=============================================================================
// Phase 8.6b: Word -> Phoneme Production (Reverse Path)
//=============================================================================

/**
 * @brief Produce phoneme sequence from word concept activation
 * WHAT: Convert word-level concept into articulatory phoneme sequence
 * WHY:  Speech production requires word->phoneme decomposition
 * HOW:  Lookup word's phoneme sequence, encode each with temporal ordering
 */
int snn_speech_bridge_produce_word(
    snn_speech_bridge_t* bridge,
    uint32_t word_pop_index,
    phoneme_t* phoneme_out,
    uint32_t* num_phonemes_out,
    uint32_t max_phonemes
);

/**
 * @brief Encode word production as spike train sequence
 * WHAT: Generate temporally-ordered spike trains for word articulation
 * WHY:  Motor cortex needs spike sequences for speech muscles
 * HOW:  Phoneme population activation in serial order with inter-phoneme gaps
 */
int snn_speech_bridge_encode_word_production(
    snn_speech_bridge_t* bridge,
    uint32_t word_pop_index,
    float start_time_ms,
    snn_spike_train_t** spike_trains_out,
    uint32_t* num_trains_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_SPEECH_BRIDGE_H */
