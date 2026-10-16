/**
 * @file nimcp_speech_jepa_bridge.h
 * @brief Speech JEPA Bridge - Encode Speech Cortex Features to JEPA Latent Space
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Bridge between Speech Cortex and JEPA latent embedding system
 * WHY:  Enable self-supervised speech learning via JEPA prediction
 * HOW:  Encode phoneme sequences → JEPA latents, predict masked segments
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * AUDIO-JEPA APPROACH:
 * --------------------
 * Similar to V-JEPA but for speech/audio:
 *
 *   1. Extract phoneme/acoustic frames
 *   2. Encode visible frames: z_ctx = Encoder(frames_visible)
 *   3. Encode target frames:  z_tgt = Encoder(frames_masked) [stop-grad]
 *   4. Predict targets:       z_pred = Predictor(z_ctx, mask_positions)
 *   5. Loss in embedding space
 *
 * SPEECH-SPECIFIC CONSIDERATIONS:
 * -------------------------------
 * - Temporal structure: Speech is inherently sequential
 * - Hierarchical: Phonemes → Syllables → Words → Prosody
 * - Co-articulation: Adjacent phonemes influence each other
 * - Speaker variation: Same phoneme varies across speakers
 *
 * MASKING STRATEGIES FOR SPEECH:
 * ------------------------------
 * - TEMPORAL: Mask contiguous time segments
 * - PHONEME: Mask entire phoneme sequences
 * - SPEAKER: Mask speaker-identifying features
 * - PREDICTIVE: Mask future given past (causal)
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Superior Temporal Gyrus (STG) and Wernicke's area implement
 * hierarchical predictive coding for speech:
 * - Lower levels predict acoustic features
 * - Higher levels predict linguistic content
 * - Prediction errors drive speech learning
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                    SPEECH JEPA BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════╣
 * ║                                                                       ║
 * ║   Speech Cortex         Speech JEPA Bridge       JEPA System         ║
 * ║   ┌─────────────────┐   ┌───────────────────┐   ┌────────────────┐  ║
 * ║   │ Phoneme Events  │──▶│ Temporal Framing  │──▶│ JEPA Latent    │  ║
 * ║   │ Formant Features│   │ Encoder (STG→WA)  │   │                │  ║
 * ║   │ Prosody         │   │ Latent Projection │   │ Predictor      │  ║
 * ║   └─────────────────┘   └───────────────────┘   │                │  ║
 * ║                                                  │ Masking        │  ║
 * ║                         ◄────── Prediction ─────│                │  ║
 * ║                                 Error           └────────────────┘  ║
 * ║                                                                       ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SPEECH_JEPA_BRIDGE_H
#define NIMCP_SPEECH_JEPA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/nimcp_speech_cortex.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_masking.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for Speech JEPA bridge */
#define BIO_MODULE_SPEECH_JEPA                  0x0E11

/** @brief Default frame size for speech JEPA (ms) */
#define SPEECH_JEPA_DEFAULT_FRAME_MS            20

/** @brief Default number of frames per sequence */
#define SPEECH_JEPA_DEFAULT_SEQUENCE_LEN        50

/** @brief Default encoder hidden dimension */
#define SPEECH_JEPA_DEFAULT_ENCODER_DIM         384

/** @brief Feature dimension per frame (phoneme + formant + prosody) */
#define SPEECH_JEPA_FRAME_FEATURES              64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Speech encoder types
 */
typedef enum {
    SPEECH_JEPA_ENCODER_LINEAR = 0,     /**< Simple linear projection */
    SPEECH_JEPA_ENCODER_MLP,            /**< 2-layer MLP encoder */
    SPEECH_JEPA_ENCODER_TRANSFORMER,    /**< Self-attention encoder */
    SPEECH_JEPA_ENCODER_RECURRENT       /**< GRU/LSTM encoder */
} speech_jepa_encoder_type_t;

/**
 * @brief Speech masking strategies
 */
typedef enum {
    SPEECH_JEPA_MASK_TEMPORAL = 0,      /**< Contiguous time segments */
    SPEECH_JEPA_MASK_PHONEME,           /**< Entire phoneme boundaries */
    SPEECH_JEPA_MASK_RANDOM,            /**< Random frame dropout */
    SPEECH_JEPA_MASK_CAUSAL,            /**< Predict future from past */
    SPEECH_JEPA_MASK_BIDIRECTIONAL      /**< Predict center from context */
} speech_jepa_mask_strategy_t;

/**
 * @brief Speech feature types to encode
 */
typedef enum {
    SPEECH_JEPA_FEAT_PHONEME = 0x01,    /**< Phoneme identity */
    SPEECH_JEPA_FEAT_FORMANT = 0x02,    /**< Formant frequencies */
    SPEECH_JEPA_FEAT_PROSODY = 0x04,    /**< Pitch, energy, duration */
    SPEECH_JEPA_FEAT_MFCC    = 0x08,    /**< Mel-frequency cepstral */
    SPEECH_JEPA_FEAT_ALL     = 0x0F     /**< All features */
} speech_jepa_feature_flags_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Speech frame representation
 */
typedef struct {
    phoneme_t phoneme;                  /**< Detected phoneme */
    float phoneme_confidence;           /**< Phoneme detection confidence */
    float formants[SPEECH_NUM_FORMANTS]; /**< Formant frequencies */
    float pitch;                        /**< Fundamental frequency (F0) */
    float energy;                       /**< Frame energy */
    float duration_ms;                  /**< Frame duration */
    uint64_t timestamp_ms;              /**< Frame timestamp */
    bool is_voiced;                     /**< Voiced speech flag */
} speech_jepa_frame_t;

/**
 * @brief Speech sequence for JEPA encoding
 */
typedef struct {
    speech_jepa_frame_t* frames;        /**< Array of frames */
    uint32_t num_frames;                /**< Number of frames */
    uint32_t max_frames;                /**< Allocated capacity */
    uint64_t start_time_ms;             /**< Sequence start time */
    uint64_t end_time_ms;               /**< Sequence end time */
} speech_jepa_sequence_t;

/**
 * @brief Speech encoder configuration
 */
typedef struct {
    speech_jepa_encoder_type_t type;    /**< Encoder architecture */
    uint32_t input_dim;                 /**< Input feature dimension */
    uint32_t hidden_dim;                /**< Hidden layer dimension */
    uint32_t output_dim;                /**< Latent dimension */
    uint32_t num_layers;                /**< Number of encoder layers */
    speech_jepa_feature_flags_t features; /**< Features to encode */
    bool use_positional_encoding;       /**< Add position information */
    bool use_layer_norm;                /**< Apply layer normalization */
} speech_jepa_encoder_config_t;

/**
 * @brief Speech JEPA bridge configuration
 */
typedef struct {
    /* Encoder config */
    speech_jepa_encoder_config_t encoder;

    /* Sequence config */
    uint32_t frame_duration_ms;         /**< Frame size in ms */
    uint32_t sequence_length;           /**< Frames per sequence */
    uint32_t sequence_stride;           /**< Stride between sequences */

    /* Masking config */
    speech_jepa_mask_strategy_t mask_strategy;
    float mask_ratio;                   /**< Fraction of frames to mask */
    uint32_t min_mask_len;              /**< Minimum consecutive masked frames */
    uint32_t max_mask_len;              /**< Maximum consecutive masked frames */

    /* Predictor config */
    jepa_predictor_config_t predictor;

    /* Training parameters */
    float learning_rate;
    float momentum;                     /**< EMA momentum for target encoder */
    bool use_target_encoder;
} speech_jepa_bridge_config_t;

/**
 * @brief Speech encoder state (MLP version)
 */
typedef struct {
    float* weights_1;
    float* bias_1;
    float* weights_2;
    float* bias_2;
    uint32_t input_dim;
    uint32_t hidden_dim;
    uint32_t output_dim;
} speech_jepa_encoder_t;

/**
 * @brief Speech JEPA bridge statistics
 */
typedef struct {
    uint64_t frames_processed;          /**< Total frames encoded */
    uint64_t sequences_processed;       /**< Total sequences */
    uint64_t predictions_made;          /**< Predictor forward passes */
    float avg_prediction_loss;          /**< Average JEPA loss */
    float min_loss;                     /**< Minimum loss */
    float phoneme_accuracy;             /**< Phoneme prediction accuracy */
} speech_jepa_stats_t;

/**
 * @brief Speech JEPA bridge state
 */
typedef struct speech_jepa_bridge {
    bridge_base_t base;                 /**< MUST be first - bridge pattern */

    /* Configuration */
    speech_jepa_bridge_config_t config;

    /* Connected systems */
    speech_cortex_t* speech_cortex;     /**< Source speech cortex */

    /* JEPA components */
    speech_jepa_encoder_t* encoder;     /**< Online encoder */
    speech_jepa_encoder_t* target_encoder; /**< Momentum target encoder */
    jepa_predictor_t* predictor;        /**< JEPA predictor */

    /* Working buffers */
    float* frame_buffer;                /**< Buffer for frame features */
    float* encoding_buffer;             /**< Buffer for encoding output */
    speech_jepa_sequence_t* current_sequence; /**< Current working sequence */

    /* Training state */
    bool training_mode;
    uint64_t training_step;
    float ema_decay;

    /* Statistics */
    speech_jepa_stats_t stats;
} speech_jepa_bridge_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default Speech JEPA configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_default_config(speech_jepa_bridge_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create Speech JEPA bridge
 *
 * WHAT: Initialize speech-to-JEPA encoding bridge
 * WHY:  Enable JEPA-based speech representation learning
 * HOW:  Create encoder, predictor, sequence buffers
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
speech_jepa_bridge_t* speech_jepa_bridge_create(
    const speech_jepa_bridge_config_t* config
);

/**
 * @brief Destroy Speech JEPA bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void speech_jepa_bridge_destroy(speech_jepa_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_reset(speech_jepa_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to speech cortex
 *
 * @param bridge Speech JEPA bridge
 * @param speech Speech cortex system
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_connect_speech_cortex(
    speech_jepa_bridge_t* bridge,
    speech_cortex_t* speech
);

/**
 * @brief Disconnect from speech cortex
 *
 * @param bridge Speech JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_disconnect_speech_cortex(
    speech_jepa_bridge_t* bridge
);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Speech JEPA bridge
 * @return true if connected to speech cortex
 */
bool speech_jepa_bridge_is_connected(const speech_jepa_bridge_t* bridge);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

/**
 * @brief Encode single speech frame to features
 *
 * WHAT: Extract features from speech frame
 * WHY:  Prepare frame for JEPA encoding
 * HOW:  Combine phoneme, formant, prosody features
 *
 * @param bridge Speech JEPA bridge
 * @param frame Input speech frame
 * @param features Output feature vector
 * @param feature_dim Feature dimension
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_extract_features(
    speech_jepa_bridge_t* bridge,
    const speech_jepa_frame_t* frame,
    float* features,
    uint32_t feature_dim
);

/**
 * @brief Encode speech sequence to JEPA latent
 *
 * WHAT: Transform speech sequence to JEPA embedding
 * WHY:  Create latent representation for prediction
 * HOW:  Apply encoder network to frame sequence
 *
 * @param bridge Speech JEPA bridge
 * @param sequence Speech frame sequence
 * @param latent Output latent embedding
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_encode(
    speech_jepa_bridge_t* bridge,
    const speech_jepa_sequence_t* sequence,
    jepa_latent_t* latent
);

/**
 * @brief Encode phoneme sequence to latent
 *
 * WHAT: Direct encoding from phoneme events
 * WHY:  Simplified interface for phoneme-level encoding
 *
 * @param bridge Speech JEPA bridge
 * @param phonemes Array of phonemes
 * @param num_phonemes Number of phonemes
 * @param latent Output latent
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_encode_phonemes(
    speech_jepa_bridge_t* bridge,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    jepa_latent_t* latent
);

/**
 * @brief Encode per-frame latents for sequence
 *
 * WHAT: Generate latent for each frame in sequence
 * WHY:  Prepare for frame-level masked prediction
 *
 * @param bridge Speech JEPA bridge
 * @param sequence Input sequence
 * @param frame_latents Output array of latents (preallocated)
 * @param num_latents Output number of latents
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_encode_frames(
    speech_jepa_bridge_t* bridge,
    const speech_jepa_sequence_t* sequence,
    jepa_latent_t** frame_latents,
    uint32_t* num_latents
);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Set training mode
 *
 * @param bridge Speech JEPA bridge
 * @param training true for training, false for inference
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_set_training(speech_jepa_bridge_t* bridge, bool training);

/**
 * @brief Perform JEPA training step on speech sequence
 *
 * WHAT: Complete JEPA training iteration
 * WHY:  Learn speech representations via prediction
 * HOW:  Mask → Encode → Predict → Compute loss → Update
 *
 * @param bridge Speech JEPA bridge
 * @param sequence Speech sequence
 * @param loss Output training loss
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_train_step(
    speech_jepa_bridge_t* bridge,
    const speech_jepa_sequence_t* sequence,
    float* loss
);

/**
 * @brief Update target encoder (EMA)
 *
 * @param bridge Speech JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_update_target_encoder(speech_jepa_bridge_t* bridge);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Predict next phoneme(s) from context
 *
 * WHAT: Language model style prediction
 * WHY:  Autoregressive speech prediction
 * HOW:  Encode context → predict next latent → decode to phoneme
 *
 * @param bridge Speech JEPA bridge
 * @param context Context latent (from past frames)
 * @param predicted_latent Output predicted latent
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_predict_next(
    speech_jepa_bridge_t* bridge,
    const jepa_latent_t* context,
    jepa_latent_t* predicted_latent
);

/**
 * @brief Predict masked frames
 *
 * WHAT: Fill in masked speech segments
 * WHY:  Core JEPA bidirectional prediction
 *
 * @param bridge Speech JEPA bridge
 * @param context_latents Visible frame latents
 * @param num_context Number of context frames
 * @param mask Mask indicating targets
 * @param predictions Output predicted latents
 * @param num_predictions Output number of predictions
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_predict_masked(
    speech_jepa_bridge_t* bridge,
    jepa_latent_t** context_latents,
    uint32_t num_context,
    const jepa_mask_t* mask,
    jepa_latent_t** predictions,
    uint32_t* num_predictions
);

/* ============================================================================
 * Sequence Management
 * ============================================================================ */

/**
 * @brief Create speech sequence
 *
 * @param max_frames Maximum frames capacity
 * @return New sequence or NULL
 */
speech_jepa_sequence_t* speech_jepa_sequence_create(uint32_t max_frames);

/**
 * @brief Destroy speech sequence
 *
 * @param sequence Sequence to destroy (NULL safe)
 */
void speech_jepa_sequence_destroy(speech_jepa_sequence_t* sequence);

/**
 * @brief Add frame to sequence
 *
 * @param sequence Target sequence
 * @param frame Frame to add
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_sequence_add_frame(
    speech_jepa_sequence_t* sequence,
    const speech_jepa_frame_t* frame
);

/**
 * @brief Clear sequence (keep allocation)
 *
 * @param sequence Sequence to clear
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_sequence_clear(speech_jepa_sequence_t* sequence);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Speech JEPA bridge
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_get_stats(
    const speech_jepa_bridge_t* bridge,
    speech_jepa_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Speech JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_reset_stats(speech_jepa_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Speech JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_connect_bio_async(speech_jepa_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Speech JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_bridge_disconnect_bio_async(speech_jepa_bridge_t* bridge);

/**
 * @brief Check bio-async connection
 *
 * @param bridge Speech JEPA bridge
 * @return true if connected
 */
bool speech_jepa_bridge_is_bio_async_connected(const speech_jepa_bridge_t* bridge);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Convert phoneme to one-hot encoding
 *
 * @param phoneme Input phoneme
 * @param encoding Output encoding [PHONEME_COUNT]
 */
void speech_jepa_phoneme_to_onehot(phoneme_t phoneme, float* encoding);

/**
 * @brief Decode latent to most likely phoneme
 *
 * @param bridge Speech JEPA bridge
 * @param latent Input latent
 * @param phoneme Output phoneme
 * @param confidence Output confidence
 * @return NIMCP_SUCCESS on success
 */
int speech_jepa_decode_phoneme(
    speech_jepa_bridge_t* bridge,
    const jepa_latent_t* latent,
    phoneme_t* phoneme,
    float* confidence
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPEECH_JEPA_BRIDGE_H */
