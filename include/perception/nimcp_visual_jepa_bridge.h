/**
 * @file nimcp_visual_jepa_bridge.h
 * @brief Visual JEPA Bridge - Encode Visual Cortex Features to JEPA Latent Space
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Bridge between Visual Cortex (V1) and JEPA latent embedding system
 * WHY:  Enable self-supervised visual learning via JEPA prediction in latent space
 * HOW:  Encode V1 features → JEPA latents, train predictor on masked patches
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * V-JEPA (Video JEPA) APPROACH:
 * -----------------------------
 * Instead of predicting raw pixels (autoencoders) or tokens (MAE), V-JEPA
 * predicts in a learned embedding space:
 *
 *   1. Encode visible patches: z_ctx = Encoder(patches_visible)
 *   2. Encode target patches:  z_tgt = Encoder(patches_masked) [stop-grad]
 *   3. Predict targets:        z_pred = Predictor(z_ctx, mask_positions)
 *   4. Loss in embedding space: L = ||z_pred - z_tgt||²
 *
 * Benefits:
 * - Learns semantic features, not low-level reconstruction
 * - More robust to noise and variations
 * - Better transfer to downstream tasks
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Visual cortex implements hierarchical prediction:
 * - V1 → V2 → V4 → IT forms generative model
 * - Higher areas predict lower area activity
 * - Prediction errors drive learning (predictive coding)
 *
 * This bridge models V1→V2 transformation as the JEPA encoder,
 * with the predictor modeling top-down predictions.
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                    VISUAL JEPA BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════╣
 * ║                                                                       ║
 * ║   Visual Cortex (V1)     Visual JEPA Bridge       JEPA System        ║
 * ║   ┌─────────────────┐   ┌───────────────────┐   ┌────────────────┐  ║
 * ║   │ Gabor Features  │──▶│ Patch Extraction  │──▶│ JEPA Latent    │  ║
 * ║   │ Pooling Output  │   │ Encoder (V1→V2)   │   │                │  ║
 * ║   │ Attention Map   │   │ Latent Projection │   │ Predictor      │  ║
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

#ifndef NIMCP_VISUAL_JEPA_BRIDGE_H
#define NIMCP_VISUAL_JEPA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/nimcp_visual_cortex.h"
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

/** @brief Bio-async module ID for Visual JEPA bridge */
#define BIO_MODULE_VISUAL_JEPA                  0x0E10

/** @brief Default patch size for visual JEPA */
#define VISUAL_JEPA_DEFAULT_PATCH_SIZE          16

/** @brief Default number of patches (grid) */
#define VISUAL_JEPA_DEFAULT_NUM_PATCHES         49  /* 7x7 grid */

/** @brief Default encoder hidden dimension */
#define VISUAL_JEPA_DEFAULT_ENCODER_DIM         512

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Visual encoder types
 */
typedef enum {
    VISUAL_JEPA_ENCODER_LINEAR = 0,     /**< Simple linear projection */
    VISUAL_JEPA_ENCODER_MLP,            /**< 2-layer MLP encoder */
    VISUAL_JEPA_ENCODER_CONV            /**< Convolutional encoder */
} visual_jepa_encoder_type_t;

/**
 * @brief Patch extraction strategies
 */
typedef enum {
    VISUAL_JEPA_PATCH_GRID = 0,         /**< Regular grid patches */
    VISUAL_JEPA_PATCH_RANDOM,           /**< Random patch locations */
    VISUAL_JEPA_PATCH_ATTENTION         /**< Attention-guided patches */
} visual_jepa_patch_strategy_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Visual JEPA encoder configuration
 */
typedef struct {
    visual_jepa_encoder_type_t type;    /**< Encoder architecture */
    uint32_t input_dim;                 /**< Visual feature dimension */
    uint32_t hidden_dim;                /**< Hidden layer dimension */
    uint32_t output_dim;                /**< Latent dimension */
    uint32_t num_layers;                /**< Number of encoder layers */
    bool use_layer_norm;                /**< Apply layer normalization */
} visual_jepa_encoder_config_t;

/**
 * @brief Patch extraction configuration
 */
typedef struct {
    visual_jepa_patch_strategy_t strategy;  /**< Patch extraction strategy */
    uint32_t patch_width;               /**< Patch width in pixels/features */
    uint32_t patch_height;              /**< Patch height */
    uint32_t num_patches_x;             /**< Grid patches in X */
    uint32_t num_patches_y;             /**< Grid patches in Y */
    uint32_t stride_x;                  /**< Patch stride X */
    uint32_t stride_y;                  /**< Patch stride Y */
} visual_jepa_patch_config_t;

/**
 * @brief Visual JEPA bridge configuration
 */
typedef struct {
    /* Encoder config */
    visual_jepa_encoder_config_t encoder;

    /* Patch config */
    visual_jepa_patch_config_t patch;

    /* Predictor config (reused from JEPA) */
    jepa_predictor_config_t predictor;

    /* Masking config (reused from JEPA) */
    jepa_mask_config_t masking;

    /* Training parameters */
    float learning_rate;                /**< Encoder learning rate */
    float momentum;                     /**< Momentum for EMA target encoder */
    bool use_target_encoder;            /**< Use momentum-updated target encoder */
} visual_jepa_bridge_config_t;

/**
 * @brief Extracted visual patch
 */
typedef struct {
    float* features;                    /**< Patch features */
    uint32_t feature_dim;               /**< Feature dimension */
    uint32_t patch_x;                   /**< Patch X position */
    uint32_t patch_y;                   /**< Patch Y position */
    float attention_score;              /**< Attention at this location */
} visual_jepa_patch_t;

/**
 * @brief Visual JEPA training batch
 */
typedef struct {
    visual_jepa_patch_t* patches;       /**< Array of patches */
    uint32_t num_patches;               /**< Number of patches */
    jepa_mask_t* mask;                  /**< Mask for training */
    jepa_latent_t** context_latents;    /**< Encoded visible patches */
    jepa_latent_t** target_latents;     /**< Encoded masked patches (targets) */
    uint32_t num_context;               /**< Number of visible patches */
    uint32_t num_targets;               /**< Number of masked patches */
} visual_jepa_batch_t;

/**
 * @brief Visual JEPA encoder state (MLP)
 */
typedef struct {
    float* weights_1;                   /**< First layer weights */
    float* bias_1;                      /**< First layer bias */
    float* weights_2;                   /**< Second layer weights */
    float* bias_2;                      /**< Second layer bias */
    uint32_t input_dim;
    uint32_t hidden_dim;
    uint32_t output_dim;
} visual_jepa_encoder_t;

/**
 * @brief Visual JEPA bridge statistics
 */
typedef struct {
    uint64_t frames_processed;          /**< Total frames encoded */
    uint64_t patches_encoded;           /**< Total patches encoded */
    uint64_t predictions_made;          /**< Predictor forward passes */
    float avg_prediction_loss;          /**< Average JEPA loss */
    float min_loss;                     /**< Minimum loss achieved */
    float encoding_time_ms;             /**< Average encoding time */
} visual_jepa_stats_t;

/**
 * @brief Visual JEPA bridge state
 */
typedef struct visual_jepa_bridge {
    bridge_base_t base;                 /**< MUST be first - bridge pattern */

    /* Configuration */
    visual_jepa_bridge_config_t config;

    /* Connected systems */
    visual_cortex_t* visual_cortex;     /**< Source visual cortex */

    /* JEPA components */
    visual_jepa_encoder_t* encoder;     /**< Online encoder */
    visual_jepa_encoder_t* target_encoder; /**< Momentum target encoder */
    jepa_predictor_t* predictor;        /**< JEPA predictor */
    jepa_mask_generator_t* mask_gen;    /**< Mask generator */

    /* Working buffers */
    float* patch_buffer;                /**< Buffer for patch extraction */
    float* encoding_buffer;             /**< Buffer for encoding output */
    uint32_t patch_buffer_size;

    /* Training state */
    bool training_mode;                 /**< Training vs inference */
    uint64_t training_step;             /**< Current training step */
    float ema_decay;                    /**< EMA decay for target encoder */

    /* Statistics */
    visual_jepa_stats_t stats;
} visual_jepa_bridge_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default Visual JEPA configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_default_config(visual_jepa_bridge_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create Visual JEPA bridge
 *
 * WHAT: Initialize visual-to-JEPA encoding bridge
 * WHY:  Enable JEPA-based visual representation learning
 * HOW:  Create encoder, predictor, masking components
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
visual_jepa_bridge_t* visual_jepa_bridge_create(
    const visual_jepa_bridge_config_t* config
);

/**
 * @brief Destroy Visual JEPA bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void visual_jepa_bridge_destroy(visual_jepa_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_reset(visual_jepa_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to visual cortex
 *
 * @param bridge Visual JEPA bridge
 * @param visual Visual cortex system
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_connect_visual_cortex(
    visual_jepa_bridge_t* bridge,
    visual_cortex_t* visual
);

/**
 * @brief Disconnect from visual cortex
 *
 * @param bridge Visual JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_disconnect_visual_cortex(
    visual_jepa_bridge_t* bridge
);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Visual JEPA bridge
 * @return true if connected to visual cortex
 */
bool visual_jepa_bridge_is_connected(const visual_jepa_bridge_t* bridge);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

/**
 * @brief Encode visual features to JEPA latent
 *
 * WHAT: Transform V1 features to JEPA embedding
 * WHY:  Create latent representation for prediction
 * HOW:  Apply encoder network to features
 *
 * @param bridge Visual JEPA bridge
 * @param features Visual features from V1
 * @param feature_dim Feature dimension
 * @param latent Output latent embedding
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_encode(
    visual_jepa_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    jepa_latent_t* latent
);

/**
 * @brief Encode entire image to patch latents
 *
 * WHAT: Extract patches and encode each to latent
 * WHY:  Prepare for JEPA masked prediction
 * HOW:  Grid extraction + per-patch encoding
 *
 * @param bridge Visual JEPA bridge
 * @param image Image data (from visual cortex or raw)
 * @param width Image width
 * @param height Image height
 * @param channels Image channels
 * @param patch_latents Output array of latents (preallocated)
 * @param num_patches Output number of patches
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_encode_patches(
    visual_jepa_bridge_t* bridge,
    const float* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    jepa_latent_t** patch_latents,
    uint32_t* num_patches
);

/**
 * @brief Encode with attention guidance
 *
 * WHAT: Encode patches prioritizing attended regions
 * WHY:  Focus encoding on salient visual content
 * HOW:  Use attention map to weight patch selection
 *
 * @param bridge Visual JEPA bridge
 * @param features Visual features
 * @param feature_dim Feature dimension per location
 * @param attention Attention map [H × W]
 * @param width Feature map width
 * @param height Feature map height
 * @param latent Output aggregated latent
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_encode_attended(
    visual_jepa_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    const float* attention,
    uint32_t width,
    uint32_t height,
    jepa_latent_t* latent
);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Set training mode
 *
 * @param bridge Visual JEPA bridge
 * @param training true for training, false for inference
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_set_training(visual_jepa_bridge_t* bridge, bool training);

/**
 * @brief Perform JEPA training step
 *
 * WHAT: Complete JEPA training iteration on visual input
 * WHY:  Learn visual representations via prediction
 * HOW:  Mask → Encode → Predict → Compute loss → Update
 *
 * @param bridge Visual JEPA bridge
 * @param features Visual features [H × W × C]
 * @param width Feature width
 * @param height Feature height
 * @param channels Feature channels
 * @param loss Output training loss
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_train_step(
    visual_jepa_bridge_t* bridge,
    const float* features,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float* loss
);

/**
 * @brief Update target encoder (EMA)
 *
 * WHAT: Update target encoder with momentum
 * WHY:  Stable targets for JEPA training
 * HOW:  target = decay * target + (1-decay) * online
 *
 * @param bridge Visual JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_update_target_encoder(visual_jepa_bridge_t* bridge);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Predict masked patch latents
 *
 * WHAT: Use predictor to fill in masked regions
 * WHY:  Core JEPA operation
 * HOW:  Predictor(context_latents) → predicted_latents
 *
 * @param bridge Visual JEPA bridge
 * @param context_latents Visible patch latents
 * @param num_context Number of context patches
 * @param mask Mask indicating target positions
 * @param predictions Output predicted latents
 * @param num_predictions Output number of predictions
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_predict_masked(
    visual_jepa_bridge_t* bridge,
    jepa_latent_t** context_latents,
    uint32_t num_context,
    const jepa_mask_t* mask,
    jepa_latent_t** predictions,
    uint32_t* num_predictions
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Visual JEPA bridge
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_get_stats(
    const visual_jepa_bridge_t* bridge,
    visual_jepa_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Visual JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_reset_stats(visual_jepa_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Visual JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_connect_bio_async(visual_jepa_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Visual JEPA bridge
 * @return NIMCP_SUCCESS on success
 */
int visual_jepa_bridge_disconnect_bio_async(visual_jepa_bridge_t* bridge);

/**
 * @brief Check bio-async connection
 *
 * @param bridge Visual JEPA bridge
 * @return true if connected
 */
bool visual_jepa_bridge_is_bio_async_connected(const visual_jepa_bridge_t* bridge);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Create batch for training
 *
 * @param num_patches Number of patches
 * @param patch_dim Patch feature dimension
 * @param latent_dim Latent dimension
 * @return New batch or NULL
 */
visual_jepa_batch_t* visual_jepa_batch_create(
    uint32_t num_patches,
    uint32_t patch_dim,
    uint32_t latent_dim
);

/**
 * @brief Destroy training batch
 *
 * @param batch Batch to destroy (NULL safe)
 */
void visual_jepa_batch_destroy(visual_jepa_batch_t* batch);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_JEPA_BRIDGE_H */
