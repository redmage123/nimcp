/**
 * @file nimcp_jepa_multimodal.h
 * @brief Multimodal JEPA - Joint Embedding Space for Vision and Speech
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Unified embedding space for visual and speech modalities
 * WHY:  Enable cross-modal reasoning and prediction
 * HOW:  Project modality-specific encodings to shared latent space
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * VL-JEPA MULTIMODAL APPROACH:
 * ----------------------------
 * Following the VL-JEPA 2 architecture:
 *
 *   1. Modality-specific encoders: Visual, Speech (from prior bridges)
 *   2. Projection to joint space: Learned linear projections
 *   3. Cross-modal prediction: Predict one modality from another
 *   4. Contrastive alignment: Pull together matched pairs
 *
 * ALIGNMENT STRATEGIES:
 * ---------------------
 * - CONTRASTIVE: NCE loss on matched/unmatched pairs
 * - PREDICTION:  Predict masked speech from visual context
 * - REGRESSION:  MSE in joint space for aligned pairs
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Superior Temporal Sulcus (STS) and Temporo-Parietal Junction (TPJ)
 * serve as multimodal convergence zones:
 *
 *   - Visual inputs from ventral stream
 *   - Auditory inputs from auditory cortex
 *   - Unified representations for speech-in-vision
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    MULTIMODAL JEPA ARCHITECTURE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                           ║
 * ║   Visual JEPA                      Speech JEPA                            ║
 * ║   ┌────────────┐                   ┌────────────┐                         ║
 * ║   │ z_visual   │                   │ z_speech   │                         ║
 * ║   └─────┬──────┘                   └──────┬─────┘                         ║
 * ║         │                                 │                               ║
 * ║         ▼                                 ▼                               ║
 * ║   ┌──────────────┐                 ┌──────────────┐                       ║
 * ║   │ W_visual     │                 │ W_speech     │                       ║
 * ║   │ (projection) │                 │ (projection) │                       ║
 * ║   └─────┬────────┘                 └──────┬───────┘                       ║
 * ║         │                                 │                               ║
 * ║         └──────────────┬──────────────────┘                               ║
 * ║                        ▼                                                  ║
 * ║               ┌───────────────────┐                                       ║
 * ║               │   JOINT SPACE     │                                       ║
 * ║               │   z_multimodal    │                                       ║
 * ║               └─────────┬─────────┘                                       ║
 * ║                         │                                                 ║
 * ║         ┌───────────────┼───────────────┐                                 ║
 * ║         ▼               ▼               ▼                                 ║
 * ║   ┌──────────┐   ┌──────────────┐ ┌────────────┐                          ║
 * ║   │Cross-Pred│   │ Alignment    │ │ Fusion     │                          ║
 * ║   │ V→S, S→V │   │ (NCE Loss)   │ │ (concat)   │                          ║
 * ║   └──────────┘   └──────────────┘ └────────────┘                          ║
 * ║                                                                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_JEPA_MULTIMODAL_H
#define NIMCP_JEPA_MULTIMODAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "perception/nimcp_visual_jepa_bridge.h"
#include "perception/nimcp_speech_jepa_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for Multimodal JEPA */
#define BIO_MODULE_JEPA_MULTIMODAL      0x0E20

/** @brief Default joint embedding dimension */
#define JEPA_MULTIMODAL_DEFAULT_JOINT_DIM   512

/** @brief Maximum number of modalities supported */
#define JEPA_MULTIMODAL_MAX_MODALITIES      4

/** @brief Temperature for contrastive loss */
#define JEPA_MULTIMODAL_DEFAULT_TEMPERATURE 0.07f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Modality types for multimodal JEPA
 */
typedef enum {
    JEPA_MM_MODALITY_VISUAL = 0,
    JEPA_MM_MODALITY_SPEECH,
    JEPA_MM_MODALITY_TEXT,
    JEPA_MM_MODALITY_ACTION,
    JEPA_MM_MODALITY_COUNT
} jepa_mm_modality_t;

/**
 * @brief Fusion strategies for combining modalities
 */
typedef enum {
    JEPA_MM_FUSION_CONCATENATE = 0,  /**< Concatenate projections */
    JEPA_MM_FUSION_AVERAGE,          /**< Average projections */
    JEPA_MM_FUSION_ATTENTION,        /**< Attention-weighted fusion */
    JEPA_MM_FUSION_GATE              /**< Gated fusion */
} jepa_mm_fusion_t;

/**
 * @brief Alignment loss types
 */
typedef enum {
    JEPA_MM_ALIGN_CONTRASTIVE = 0,   /**< NCE contrastive loss */
    JEPA_MM_ALIGN_MSE,               /**< Mean squared error */
    JEPA_MM_ALIGN_COSINE,            /**< Cosine similarity */
    JEPA_MM_ALIGN_BARLOW_TWINS       /**< Barlow Twins redundancy reduction */
} jepa_mm_alignment_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Projection layer configuration
 */
typedef struct {
    uint32_t input_dim;              /**< Input dimension (modality-specific) */
    uint32_t output_dim;             /**< Output (joint) dimension */
    uint32_t hidden_dim;             /**< Hidden dimension (0 = linear) */
    bool use_layer_norm;             /**< Apply layer normalization */
    bool use_bias;                   /**< Include bias terms */
} jepa_mm_projection_config_t;

/**
 * @brief Multimodal JEPA configuration
 */
typedef struct {
    /* Joint space config */
    uint32_t joint_dim;              /**< Joint embedding dimension */

    /* Per-modality projection configs */
    jepa_mm_projection_config_t visual_proj;
    jepa_mm_projection_config_t speech_proj;

    /* Fusion config */
    jepa_mm_fusion_t fusion_type;

    /* Alignment config */
    jepa_mm_alignment_t alignment_type;
    float temperature;               /**< Contrastive temperature */
    float alignment_weight;          /**< Weight for alignment loss */

    /* Cross-modal prediction config */
    jepa_predictor_config_t cross_predictor;
    bool enable_visual_to_speech;
    bool enable_speech_to_visual;

    /* Training parameters */
    float learning_rate;
    float momentum;
} jepa_multimodal_config_t;

/**
 * @brief Projection layer state
 */
typedef struct {
    float* weights;                  /**< Projection weights [input x output] */
    float* bias;                     /**< Bias [output] (if enabled) */
    float* hidden_weights;           /**< Hidden layer weights (if hidden_dim > 0) */
    float* hidden_bias;              /**< Hidden layer bias */
    uint32_t input_dim;
    uint32_t hidden_dim;
    uint32_t output_dim;
    bool use_bias;
    bool use_layer_norm;
} jepa_mm_projection_t;

/**
 * @brief Multimodal pair for training
 */
typedef struct {
    jepa_latent_t* visual;           /**< Visual embedding */
    jepa_latent_t* speech;           /**< Speech embedding */
    float similarity;                /**< Ground truth similarity (0-1) */
    bool is_matched;                 /**< True if positive pair */
} jepa_mm_pair_t;

/**
 * @brief Multimodal batch for training
 */
typedef struct {
    jepa_mm_pair_t* pairs;           /**< Array of pairs */
    uint32_t num_pairs;              /**< Number of pairs */
    uint32_t num_positive;           /**< Number of positive pairs */
} jepa_mm_batch_t;

/**
 * @brief Multimodal JEPA statistics
 */
typedef struct {
    /* Encoding stats */
    uint64_t visual_encodings;       /**< Visual projections made */
    uint64_t speech_encodings;       /**< Speech projections made */
    uint64_t fusions_performed;      /**< Multimodal fusions */

    /* Cross-modal stats */
    uint64_t visual_to_speech_preds; /**< V→S predictions */
    uint64_t speech_to_visual_preds; /**< S→V predictions */
    float avg_cross_pred_loss;       /**< Average cross-prediction loss */

    /* Alignment stats */
    uint64_t alignment_steps;        /**< Alignment training steps */
    float avg_alignment_loss;        /**< Average alignment loss */
    float avg_similarity;            /**< Average positive pair similarity */
} jepa_multimodal_stats_t;

/**
 * @brief Multimodal JEPA bridge state
 */
typedef struct jepa_multimodal {
    bridge_base_t base;              /**< MUST be first - bridge pattern */

    /* Configuration */
    jepa_multimodal_config_t config;

    /* Connected encoders */
    visual_jepa_bridge_t* visual_encoder;
    speech_jepa_bridge_t* speech_encoder;

    /* Projection layers */
    jepa_mm_projection_t* visual_projection;
    jepa_mm_projection_t* speech_projection;

    /* Cross-modal predictors */
    jepa_predictor_t* visual_to_speech;
    jepa_predictor_t* speech_to_visual;

    /* Working buffers */
    float* visual_buffer;            /**< Buffer for visual projection */
    float* speech_buffer;            /**< Buffer for speech projection */
    float* fused_buffer;             /**< Buffer for fused embedding */

    /* Training state */
    bool training_mode;
    uint64_t training_step;

    /* Statistics */
    jepa_multimodal_stats_t stats;
} jepa_multimodal_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default multimodal JEPA configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_default_config(jepa_multimodal_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create multimodal JEPA system
 *
 * WHAT: Initialize multimodal joint embedding space
 * WHY:  Enable cross-modal understanding and prediction
 * HOW:  Create projection layers and predictors
 *
 * @param config Configuration (NULL for defaults)
 * @return New multimodal system or NULL on failure
 */
jepa_multimodal_t* jepa_multimodal_create(
    const jepa_multimodal_config_t* config
);

/**
 * @brief Destroy multimodal JEPA system
 *
 * @param mm System to destroy (NULL safe)
 */
void jepa_multimodal_destroy(jepa_multimodal_t* mm);

/**
 * @brief Reset multimodal system state
 *
 * @param mm Multimodal system
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_reset(jepa_multimodal_t* mm);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect visual JEPA encoder
 *
 * @param mm Multimodal system
 * @param visual Visual JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_connect_visual(
    jepa_multimodal_t* mm,
    visual_jepa_bridge_t* visual
);

/**
 * @brief Connect speech JEPA encoder
 *
 * @param mm Multimodal system
 * @param speech Speech JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_connect_speech(
    jepa_multimodal_t* mm,
    speech_jepa_bridge_t* speech
);

/**
 * @brief Disconnect all encoders
 *
 * @param mm Multimodal system
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_disconnect_all(jepa_multimodal_t* mm);

/**
 * @brief Check if fully connected
 *
 * @param mm Multimodal system
 * @return true if both visual and speech connected
 */
bool jepa_multimodal_is_connected(const jepa_multimodal_t* mm);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

/**
 * @brief Project visual latent to joint space
 *
 * WHAT: Map visual JEPA embedding to multimodal space
 * WHY:  Enable comparison with other modalities
 * HOW:  Apply learned projection layer
 *
 * @param mm Multimodal system
 * @param visual_latent Input visual embedding
 * @param joint_latent Output joint space embedding
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_encode_visual(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    jepa_latent_t* joint_latent
);

/**
 * @brief Project speech latent to joint space
 *
 * WHAT: Map speech JEPA embedding to multimodal space
 * WHY:  Enable comparison with other modalities
 * HOW:  Apply learned projection layer
 *
 * @param mm Multimodal system
 * @param speech_latent Input speech embedding
 * @param joint_latent Output joint space embedding
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_encode_speech(
    jepa_multimodal_t* mm,
    const jepa_latent_t* speech_latent,
    jepa_latent_t* joint_latent
);

/**
 * @brief Fuse visual and speech embeddings
 *
 * WHAT: Combine both modalities into unified representation
 * WHY:  Joint reasoning over audio-visual content
 * HOW:  Apply configured fusion strategy
 *
 * @param mm Multimodal system
 * @param visual_latent Visual embedding
 * @param speech_latent Speech embedding
 * @param fused_latent Output fused embedding
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_fuse(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    const jepa_latent_t* speech_latent,
    jepa_latent_t* fused_latent
);

/* ============================================================================
 * Cross-Modal Prediction API
 * ============================================================================ */

/**
 * @brief Predict speech latent from visual context
 *
 * WHAT: Lip-reading style prediction
 * WHY:  Learn visual-speech correspondence
 * HOW:  Apply visual-to-speech predictor
 *
 * @param mm Multimodal system
 * @param visual_latent Input visual context
 * @param predicted_speech Output predicted speech embedding
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_predict_speech_from_visual(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    jepa_latent_t* predicted_speech
);

/**
 * @brief Predict visual latent from speech context
 *
 * WHAT: Infer visual content from audio
 * WHY:  Learn speech-visual correspondence
 * HOW:  Apply speech-to-visual predictor
 *
 * @param mm Multimodal system
 * @param speech_latent Input speech context
 * @param predicted_visual Output predicted visual embedding
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_predict_visual_from_speech(
    jepa_multimodal_t* mm,
    const jepa_latent_t* speech_latent,
    jepa_latent_t* predicted_visual
);

/* ============================================================================
 * Similarity API
 * ============================================================================ */

/**
 * @brief Compute cross-modal similarity
 *
 * WHAT: Measure alignment between visual and speech
 * WHY:  Quantify how well modalities correspond
 * HOW:  Cosine similarity in joint space
 *
 * @param mm Multimodal system
 * @param visual_latent Visual embedding
 * @param speech_latent Speech embedding
 * @param similarity Output similarity score [0,1]
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_similarity(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    const jepa_latent_t* speech_latent,
    float* similarity
);

/**
 * @brief Compute batch similarities (for contrastive learning)
 *
 * @param mm Multimodal system
 * @param visual_latents Array of visual embeddings
 * @param speech_latents Array of speech embeddings
 * @param num_samples Number of samples
 * @param similarity_matrix Output [num_samples x num_samples]
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_batch_similarity(
    jepa_multimodal_t* mm,
    jepa_latent_t** visual_latents,
    jepa_latent_t** speech_latents,
    uint32_t num_samples,
    float* similarity_matrix
);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Set training mode
 *
 * @param mm Multimodal system
 * @param training true for training, false for inference
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_set_training(jepa_multimodal_t* mm, bool training);

/**
 * @brief Alignment training step
 *
 * WHAT: Train projection layers for alignment
 * WHY:  Learn to map modalities to common space
 * HOW:  Contrastive/MSE loss on matched pairs
 *
 * @param mm Multimodal system
 * @param batch Training batch of pairs
 * @param loss Output training loss
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_align_step(
    jepa_multimodal_t* mm,
    const jepa_mm_batch_t* batch,
    float* loss
);

/**
 * @brief Cross-modal prediction training step
 *
 * WHAT: Train cross-modal predictors
 * WHY:  Learn to predict one modality from another
 * HOW:  MSE in joint space
 *
 * @param mm Multimodal system
 * @param visual_latent Visual embedding
 * @param speech_latent Speech embedding
 * @param loss Output training loss
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_cross_pred_step(
    jepa_multimodal_t* mm,
    const jepa_latent_t* visual_latent,
    const jepa_latent_t* speech_latent,
    float* loss
);

/* ============================================================================
 * Batch Management
 * ============================================================================ */

/**
 * @brief Create multimodal batch
 *
 * @param max_pairs Maximum number of pairs
 * @return New batch or NULL
 */
jepa_mm_batch_t* jepa_mm_batch_create(uint32_t max_pairs);

/**
 * @brief Destroy multimodal batch
 *
 * @param batch Batch to destroy (NULL safe)
 */
void jepa_mm_batch_destroy(jepa_mm_batch_t* batch);

/**
 * @brief Add pair to batch
 *
 * @param batch Target batch
 * @param visual Visual embedding
 * @param speech Speech embedding
 * @param is_matched Whether pair is matched
 * @return NIMCP_SUCCESS on success
 */
int jepa_mm_batch_add_pair(
    jepa_mm_batch_t* batch,
    const jepa_latent_t* visual,
    const jepa_latent_t* speech,
    bool is_matched
);

/**
 * @brief Clear batch (keep allocation)
 *
 * @param batch Batch to clear
 * @return NIMCP_SUCCESS on success
 */
int jepa_mm_batch_clear(jepa_mm_batch_t* batch);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get multimodal statistics
 *
 * @param mm Multimodal system
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_get_stats(
    const jepa_multimodal_t* mm,
    jepa_multimodal_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param mm Multimodal system
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_reset_stats(jepa_multimodal_t* mm);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param mm Multimodal system
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_connect_bio_async(jepa_multimodal_t* mm);

/**
 * @brief Disconnect from bio-async router
 *
 * @param mm Multimodal system
 * @return NIMCP_SUCCESS on success
 */
int jepa_multimodal_disconnect_bio_async(jepa_multimodal_t* mm);

/**
 * @brief Check bio-async connection
 *
 * @param mm Multimodal system
 * @return true if connected
 */
bool jepa_multimodal_is_bio_async_connected(const jepa_multimodal_t* mm);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_MULTIMODAL_H */
