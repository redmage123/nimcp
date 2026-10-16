/**
 * @file nimcp_audio_cortex.h
 * @brief Biologically-inspired auditory processing system
 *
 * WHAT: Audio cortex with cochlear processing and temporal analysis
 * WHY:  Enable auditory perception and memory in NIMCP
 * HOW:  FFT-based frequency analysis + temporal pattern recognition
 *
 * Mimics biological auditory pathway:
 * - Cochlea: Frequency decomposition (basilar membrane simulation)
 * - Primary Auditory Cortex (A1): Tonotopic feature extraction
 * - Higher-order areas: Temporal pattern recognition
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6
 */

#ifndef NIMCP_AUDIO_CORTEX_H
#define NIMCP_AUDIO_CORTEX_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Imagination engine integration */
#include "cognitive/imagination/nimcp_imagination_callbacks.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration for brain integration
typedef struct brain_struct* brain_t;

// Forward declarations for imagination engine integration
typedef struct imagination_engine imagination_engine_t;
typedef struct imagination_scenario imagination_scenario_t;

//=============================================================================
// Neuromodulation Structures
//=============================================================================

// NOTE: phasic_tonic_state_t and receptor_expression_t are defined in
// nimcp_visual_cortex.h to avoid duplicate definitions. Include that header
// first if you need these types in audio cortex.

//=============================================================================
// Configuration Constants
//=============================================================================

#define AUDIO_MAX_SAMPLE_RATE 48000
#define AUDIO_MIN_SAMPLE_RATE 8000
#define AUDIO_MAX_CHANNELS 2
#define AUDIO_MAX_MEMORIES 1000
#define AUDIO_MAX_FREQ_BINS 512

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Audio cortex configuration
 */
typedef struct {
    uint32_t sample_rate;        ///< Sample rate (Hz)
    uint32_t frame_size;         ///< Frame size (samples)
    uint32_t num_freq_bins;      ///< Number of frequency bins
    uint32_t num_mel_filters;    ///< Number of mel-scale filters
    uint32_t num_mfcc;           ///< Number of MFCC coefficients
    uint8_t num_channels;        ///< 1=mono, 2=stereo
    uint32_t feature_dim;        ///< Feature vector dimension
    bool enable_attention;       ///< Enable attention mechanisms
    bool enable_memory;          ///< Enable auditory memory

    // NIMCP 2.7 Phase 8.5: Fractal Topology Integration
    bool enable_fractal_topology;  /**< Enable scale-free topology within A1 */
    float hub_ratio;               /**< Fraction of hub neurons (0.1-0.2), default: 0.15 */
    float power_law_gamma;         /**< Power-law exponent (-2 to -3), default: -2.1 */
    uint32_t internal_neurons;     /**< Number of internal A1 neurons, default: num_mel_filters * 10 */

    // Bio-async communication
    bool enable_bio_async;         /**< Enable bio-async messaging */

    // Second messenger cascade integration
    bool enable_second_messengers; /**< Enable second messenger cascades */
} audio_cortex_config_t;

/**
 * @brief Auditory memory entry
 */
typedef struct {
    float* features;             ///< Audio feature vector
    float salience;              ///< Memory salience [0,1]
    uint64_t timestamp;          ///< When memory was created
    char context[64];            ///< Context label
} auditory_memory_t;

/**
 * @brief Attention map for audio (frequency/time)
 */
typedef struct {
    float* values;               ///< Attention weights
    uint32_t num_freq;           ///< Frequency bins
    uint32_t num_time;           ///< Time frames
} audio_attention_map_t;

/**
 * @brief Audio cortex instance (opaque)
 */
typedef struct audio_cortex audio_cortex_t;

/**
 * @brief Audio cortex statistics
 */
typedef struct {
    uint64_t frames_processed;   ///< Total frames processed
    uint32_t memories_stored;    ///< Auditory memories stored
    float avg_processing_time;   ///< Avg processing time (ms)
} audio_cortex_stats_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create audio cortex instance
 * @param config Configuration parameters
 * @return Audio cortex instance or NULL on failure
 */
audio_cortex_t* audio_cortex_create(const audio_cortex_config_t* config);

/**
 * @brief Destroy audio cortex instance
 * @param cortex Audio cortex to destroy
 */
void audio_cortex_destroy(audio_cortex_t* cortex);

/**
 * @brief Process audio frame and extract features
 *
 * @param cortex Audio cortex instance
 * @param audio_data Raw audio samples (float32)
 * @param num_samples Number of samples
 * @param num_channels Number of channels (1 or 2)
 * @param features Output feature vector (must be pre-allocated)
 * @return true on success, false on failure
 */
bool audio_cortex_process(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    uint8_t num_channels,
    float* features
);

/**
 * @brief Get audio cortex statistics
 * @param cortex Audio cortex instance
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool audio_cortex_get_stats(
    const audio_cortex_t* cortex,
    audio_cortex_stats_t* stats
);

//=============================================================================
// Cochlear Processing (Frequency Analysis)
//=============================================================================

/**
 * @brief Compute power spectrum using FFT
 *
 * @param cortex Audio cortex instance
 * @param audio_data Time-domain audio samples
 * @param num_samples Number of samples
 * @param spectrum Output power spectrum (must be pre-allocated)
 * @return true on success, false on failure
 */
bool audio_cortex_compute_spectrum(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* spectrum
);

/**
 * @brief Apply mel-scale filterbank
 *
 * @param cortex Audio cortex instance
 * @param spectrum Power spectrum
 * @param num_bins Number of frequency bins
 * @param mel_features Output mel-scale features
 * @return true on success, false on failure
 */
bool audio_cortex_compute_mel_features(
    audio_cortex_t* cortex,
    const float* spectrum,
    uint32_t num_bins,
    float* mel_features
);

/**
 * @brief Compute MFCC features
 *
 * @param cortex Audio cortex instance
 * @param mel_features Mel-scale features
 * @param num_mel Number of mel filters
 * @param mfcc Output MFCC coefficients
 * @return true on success, false on failure
 */
bool audio_cortex_compute_mfcc(
    audio_cortex_t* cortex,
    const float* mel_features,
    uint32_t num_mel,
    float* mfcc
);

//=============================================================================
// Attention Mechanisms
//=============================================================================

/**
 * @brief Create audio attention map
 * @param num_freq Number of frequency bins
 * @param num_time Number of time frames
 * @return Attention map or NULL on failure
 */
audio_attention_map_t* audio_attention_map_create(
    uint32_t num_freq,
    uint32_t num_time
);

/**
 * @brief Destroy audio attention map
 * @param map Attention map to destroy
 */
void audio_attention_map_destroy(audio_attention_map_t* map);

/**
 * @brief Compute attention map from audio features
 *
 * @param cortex Audio cortex instance
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param attn_map Output attention map
 * @return true on success, false on failure
 */
bool audio_cortex_compute_attention(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    audio_attention_map_t* attn_map
);

//=============================================================================
// Auditory Memory
//=============================================================================

/**
 * @brief Store auditory memory
 *
 * @param cortex Audio cortex instance
 * @param features Audio feature vector
 * @param salience Memory salience [0,1]
 * @return true on success, false on failure
 */
bool audio_cortex_store_memory(
    audio_cortex_t* cortex,
    const float* features,
    float salience
);

/**
 * @brief Recall similar auditory memories
 *
 * @param cortex Audio cortex instance
 * @param query_features Query feature vector
 * @param top_k Number of memories to retrieve
 * @param memories Output array of memory pointers
 * @param num_recalled Number of memories actually recalled
 * @return true on success, false on failure
 *
 * @note Caller must free the memories array with nimcp_free()
 */
bool audio_cortex_recall_memory(
    audio_cortex_t* cortex,
    const float* query_features,
    int top_k,
    auditory_memory_t*** memories,
    int* num_recalled
);

//=============================================================================
// Brain Integration Helpers
//=============================================================================

/**
 * @brief Compute novelty score for curiosity system
 *
 * Returns how novel/unfamiliar an audio pattern is based on
 * similarity to stored auditory memories.
 *
 * @param cortex Audio cortex instance
 * @param features Audio feature vector
 * @return Novelty score [0,1], 0=familiar, 1=novel
 */
float audio_cortex_compute_novelty(
    audio_cortex_t* cortex,
    const float* features
);

/**
 * @brief Get peak attention (most salient frequency/time)
 *
 * @param attn_map Attention map
 * @param max_freq Output: frequency bin with max attention
 * @param max_time Output: time frame with max attention
 * @param max_value Output: attention value at peak
 * @return true on success, false on failure
 */
bool audio_cortex_get_attention_peak(
    const audio_attention_map_t* attn_map,
    uint32_t* max_freq,
    uint32_t* max_time,
    float* max_value
);

/**
 * @brief Consolidate auditory memory
 *
 * Integrates with memory consolidation system (hippocampus, etc.)
 *
 * @param cortex Audio cortex instance
 * @param features Audio feature vector
 * @param salience Memory salience [0,1]
 * @param context Context label for memory
 * @return true on success, false on failure
 */
bool audio_cortex_consolidate_memory(
    audio_cortex_t* cortex,
    const float* features,
    float salience,
    const char* context
);

//=============================================================================
// Temporal Processing
//=============================================================================

/**
 * @brief Detect temporal patterns (onset, offset, rhythm)
 *
 * @param cortex Audio cortex instance
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param onset_detected Output: onset detected flag
 * @param offset_detected Output: offset detected flag
 * @return true on success, false on failure
 */
bool audio_cortex_detect_temporal_events(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    bool* onset_detected,
    bool* offset_detected
);

/**
 * @brief Compute temporal envelope
 *
 * @param cortex Audio cortex instance
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param envelope Output envelope (must be pre-allocated)
 * @return true on success, false on failure
 */
bool audio_cortex_compute_envelope(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* envelope
);

/**
 * @brief Associate brain with audio cortex for neuromodulation
 *
 * WHAT: Set brain reference for ACh + 5-HT modulation
 * WHY:  Enable neurochemical modulation of auditory processing
 * HOW:  Store brain pointer for neurotransmitter reading
 *
 * @param cortex Audio cortex instance
 * @param brain Brain instance (or NULL to clear)
 *
 * BIOLOGY:
 * - Acetylcholine enhances frequency selectivity (cocktail party effect)
 * - Serotonin gates auditory sensitivity (prevent sensory overload)
 *
 * CLINICAL EXAMPLES:
 * - Autism (low 5-HT): Sound sensitivity, overwhelmed by noise
 * - ADHD (low ACh): Poor auditory attention, can't filter background
 */
void audio_cortex_set_brain(audio_cortex_t* cortex, brain_t brain);

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Get speech salience from audio features
 *
 * WHAT: Query how speech-like current audio is
 * WHY:  Speech cortex can prioritize speech processing
 * HOW:  Return energy concentration in speech frequencies (300-3400 Hz)
 *
 * BIOLOGY: Superior temporal gyrus (STG) receives speech-tuned signals from A1
 *
 * @param cortex Audio cortex instance
 * @param features Audio feature vector from recent processing
 * @param num_features Number of features
 * @return Speech salience [0, 1] (0=noise, 1=clear speech)
 */
float audio_cortex_get_speech_salience(audio_cortex_t* cortex,
                                        const float* features,
                                        uint32_t num_features);

/**
 * @brief Activate speech processing mode
 *
 * WHAT: Signal that speech detected, optimize for phoneme extraction
 * WHY:  Speech detection triggers specialized processing
 * HOW:  Prime frequency bands and temporal resolution for speech
 *
 * @param cortex Audio cortex instance
 */
void audio_cortex_activate_speech_mode(audio_cortex_t* cortex);

//=============================================================================
// Bio-Async Communication API
//=============================================================================

/**
 * @brief Get bio-async module context
 *
 * @param cortex Audio cortex instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t audio_cortex_get_bio_context(audio_cortex_t* cortex);

/**
 * @brief Process pending bio-async messages
 *
 * @param cortex Audio cortex instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t audio_cortex_process_bio_messages(audio_cortex_t* cortex, uint32_t max_messages);

/**
 * @brief Broadcast audio input detection via bio-async
 *
 * @param cortex Audio cortex instance
 * @param features Audio features
 * @param num_features Number of features
 * @param salience Audio salience (0-1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t audio_cortex_broadcast_input(
    audio_cortex_t* cortex,
    const float* features,
    uint32_t num_features,
    float salience
);

/**
 * @brief Broadcast speech detected notification
 *
 * @param cortex Audio cortex instance
 * @param speech_salience Speech likelihood (0-1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t audio_cortex_broadcast_speech_detected(
    audio_cortex_t* cortex,
    float speech_salience
);

//=============================================================================
// Second Messenger Cascade Integration
//=============================================================================

/**
 * @brief Activate receptor cascade in audio cortex neuron
 *
 * WHAT: Trigger second messenger cascade via receptor activation
 * WHY:  Neuromodulators activate GPCRs -> cascades modulate audio processing
 * HOW:  Route to appropriate G-protein pathway (Gs/Gi/Gq)
 *
 * BIOLOGY:
 * - Dopamine D1 (Gs) -> cAMP -> PKA: Modulates frequency tuning
 * - Acetylcholine M4 (Gq) -> IP3/DAG -> PKC: Enhances frequency selectivity
 * - Serotonin 5-HT2A (Gq) -> IP3/DAG -> PKC: Gates auditory sensitivity
 *
 * @param cortex Audio cortex instance
 * @param neuron_id Neuron ID (maps to frequency bin)
 * @param receptor Receptor type to activate
 * @param occupancy Receptor occupancy [0, 1]
 * @param timestamp_ms Current timestamp
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t audio_cortex_trigger_receptor(
    audio_cortex_t* cortex,
    uint32_t neuron_id,
    uint32_t receptor,
    float occupancy,
    uint64_t timestamp_ms
);

/**
 * @brief Get second messenger cascade state for neuron
 *
 * WHAT: Query cascade state for specific frequency neuron
 * WHY:  Monitor kinase activities and modulation levels
 * HOW:  Return copy of cascade state from second messenger system
 *
 * @param cortex Audio cortex instance
 * @param neuron_id Neuron ID (frequency bin index)
 * @param state Output state buffer
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t audio_cortex_get_second_messenger_state(
    audio_cortex_t* cortex,
    uint32_t neuron_id,
    void* state
);

//=============================================================================
// Training Interface API (CNN-Cortex Integration)
//=============================================================================

/**
 * @brief Audio cortex training state for gradient feedback
 *
 * WHAT: Cached activations and outputs from audio cortex forward pass
 * WHY:  Needed for gradient feedback to audio cortex internal network
 * HOW:  Populated during audio_cortex_process, queried by CNN trainer
 *
 * BIOLOGICAL BASIS: Models the cached activity patterns in A1 that
 * serve as the substrate for top-down attentional modulation of plasticity.
 */
typedef struct {
    float* mel_features;             /**< Cached mel filterbank features */
    uint32_t num_mel_filters;        /**< Number of mel filters */
    float* mfcc_features;            /**< Cached MFCC coefficients */
    uint32_t num_mfcc;               /**< Number of MFCCs */
    float quality;                   /**< Audio signal quality [0-1] */
    float speech_salience;           /**< Speech salience [0-1] */
    float temporal_coherence;        /**< Temporal coherence [0-1] */
    uint64_t timestamp_ms;           /**< When state was captured */
    bool valid;                      /**< State validity flag */
} audio_training_state_t;

/**
 * @brief Get training state for gradient feedback
 *
 * WHAT: Retrieve cached activations from last forward pass
 * WHY:  Enables gradient-based feedback for internal network plasticity
 * HOW:  Copies internal cached state to output structure
 *
 * @param cortex Audio cortex instance
 * @param state Output training state structure
 * @return 0 on success, negative on error
 */
int audio_cortex_get_training_state(
    audio_cortex_t* cortex,
    audio_training_state_t* state
);

/**
 * @brief Apply gradient feedback for internal network plasticity
 *
 * WHAT: Receive gradient signals from CNN and modulate A1 plasticity
 * WHY:  Enables top-down learning signal from CNN training
 * HOW:  Convert gradients to plasticity modulation signals
 *
 * BIOLOGICAL BASIS: Models feedback projections from higher auditory
 * areas (STG, PFC) to A1, modulating plasticity via attention signals.
 *
 * @param cortex Audio cortex instance
 * @param gradients Gradient values (one per output feature)
 * @param gradient_size Number of gradient values
 * @param scale Scaling factor for gradient signals [0-1]
 * @return 0 on success, negative on error
 */
int audio_cortex_apply_gradient_feedback(
    audio_cortex_t* cortex,
    const float* gradients,
    uint32_t gradient_size,
    float scale
);

/**
 * @brief Extract features as tensor for CNN training
 *
 * WHAT: Process audio and return features as nimcp_tensor_t
 * WHY:  Direct integration with tensor-based CNN training
 * HOW:  Wrapper around audio_cortex_process with tensor output
 *
 * @param cortex Audio cortex instance
 * @param audio Input audio samples
 * @param num_samples Number of audio samples
 * @param num_channels Number of audio channels
 * @param features Output tensor (allocated by function, caller destroys)
 * @return 0 on success, negative on error
 *
 * @note Caller is responsible for destroying the returned tensor
 */
struct nimcp_tensor;
int audio_cortex_extract_features_tensor(
    audio_cortex_t* cortex,
    const float* audio,
    uint32_t num_samples,
    uint8_t num_channels,
    struct nimcp_tensor** features
);

/**
 * @brief Get feature dimension for CNN input shape configuration
 *
 * WHAT: Query the output feature dimensionality
 * WHY:  CNN trainer needs to know input shape from cortex
 * HOW:  Returns configured feature_dim from cortex
 *
 * @param cortex Audio cortex instance
 * @return Feature dimension, or 0 on error
 */
uint32_t audio_cortex_get_feature_dim(const audio_cortex_t* cortex);

/**
 * @brief Enable/disable activation caching for training
 *
 * WHAT: Toggle caching of intermediate activations
 * WHY:  Caching needed for gradient feedback, but uses memory
 * HOW:  Sets internal flag to cache mel/mfcc features
 *
 * @param cortex Audio cortex instance
 * @param enable true to enable caching, false to disable
 * @return 0 on success, negative on error
 */
int audio_cortex_set_training_mode(audio_cortex_t* cortex, bool enable);

/**
 * @brief Check if training mode is enabled
 *
 * @param cortex Audio cortex instance
 * @return true if training mode enabled
 */
bool audio_cortex_is_training_mode(const audio_cortex_t* cortex);

//=============================================================================
// Imagination Engine Integration
//=============================================================================

/**
 * @brief Connect imagination engine to audio cortex
 *
 * WHAT: Establish bidirectional link between imagination and audio processing
 * WHY:  Enable imagined audio generation and imagination-guided perception
 * HOW:  Store engine reference and register audio cortex as imagination target
 *
 * BIOLOGICAL BASIS: Models the connection between auditory cortex and
 * prefrontal/hippocampal imagination systems. Enables "hearing in your head"
 * - auditory imagery during daydreaming, memory recall, and planning.
 *
 * @param cortex Audio cortex instance
 * @param engine Imagination engine to connect
 * @return 0 on success, negative on error
 */
int audio_cortex_connect_imagination(audio_cortex_t* cortex, imagination_engine_t* engine);

/**
 * @brief Set callback for imagination-generated audio events
 *
 * WHAT: Register callback for when imagination generates audio content
 * WHY:  Enable cortex to process imagined audio like real input
 * HOW:  Store callback and user data, invoke during imagination playback
 *
 * BIOLOGICAL BASIS: Models the feedback projections from imagination
 * systems that can activate auditory cortex during mental imagery,
 * producing subjective "hearing" without external stimulus.
 *
 * @param cortex Audio cortex instance
 * @param cb Callback function for imagination events
 * @param user_data User data passed to callback
 * @return 0 on success, negative on error
 */
int audio_cortex_set_imagination_callback(audio_cortex_t* cortex, imagination_audio_callback_t cb, void* user_data);

/**
 * @brief Generate audio features from imagination latent representation
 *
 * WHAT: Decode latent vector into audio cortex features
 * WHY:  Enable imagination to drive audio perception pathways
 * HOW:  Use inverse feature mapping to generate mel/MFCC from latent
 *
 * BIOLOGICAL BASIS: Models the generative pathway where imagination
 * latent representations are decoded into sensory-specific formats.
 * Similar to how we can "hear" imagined music or speech.
 *
 * @param cortex Audio cortex instance
 * @param latent Latent representation tensor from imagination engine
 * @return 0 on success, negative on error
 */
int audio_cortex_generate_from_imagination(audio_cortex_t* cortex, struct nimcp_tensor* latent);

/**
 * @brief Render imagined audio from complete scenario
 *
 * WHAT: Generate full audio experience from imagination scenario
 * WHY:  Enable rich multi-sensory imagination with audio component
 * HOW:  Extract audio modality from scenario and render via cortex
 *
 * BIOLOGICAL BASIS: Models how imagination scenarios (memories,
 * fantasies, plans) include auditory components that are rendered
 * through auditory cortex to create subjective audio experience.
 *
 * @param cortex Audio cortex instance
 * @param scenario Imagination scenario containing audio modality
 * @return 0 on success, negative on error
 */
int audio_cortex_render_imagined_audio(audio_cortex_t* cortex, imagination_scenario_t* scenario);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_AUDIO_CORTEX_H
