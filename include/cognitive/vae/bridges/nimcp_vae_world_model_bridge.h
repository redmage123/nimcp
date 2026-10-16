/**
 * @file nimcp_vae_world_model_bridge.h
 * @brief Bridge between VAE and World Model (JEPA) for Latent Prediction
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE with world model for latent-space prediction
 *
 * WHY:  VAE + World Model enables:
 *       - Entity states encoded as latent vectors
 *       - Multimodal fusion in latent space
 *       - Prediction in latent space (more efficient)
 *       - Decode predictions to observations
 *       - Cross-modal attention in latent dimensions
 *
 * HOW:  Bridge combines VAE encoding with JEPA prediction:
 *       - Encode: Observation → VAE latent
 *       - Predict: JEPA predicts next latent state
 *       - Decode: Predicted latent → observation
 *       - Fuse: Multiple modality latents → unified representation
 *
 * WORLD MODEL INTEGRATION:
 * ========================
 * - JEPA: Joint Embedding Predictive Architecture
 * - Multimodal fusion (visual, audio, tactile, etc.)
 * - Entity tracking and prediction
 * - Cross-modal attention
 *
 * BIO_MODULE: 0x1F1A (VAE-World Model Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_WORLD_MODEL_BRIDGE_H
#define NIMCP_VAE_WORLD_MODEL_BRIDGE_H

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

#define VAE_WORLD_BRIDGE_VERSION        "1.0.0"
#define BIO_MODULE_VAE_WORLD_BRIDGE     0x1F1A

/** Maximum modalities for fusion */
#define VAE_WORLD_MAX_MODALITIES        10

/** Maximum entities to track */
#define VAE_WORLD_MAX_ENTITIES          64

/** Maximum prediction horizon */
#define VAE_WORLD_MAX_HORIZON           100

/** Error code range (32510-32519) */
#define NIMCP_ERROR_VAE_WORLD_BASE          32510
#define NIMCP_ERROR_VAE_WORLD_NULL          32511
#define NIMCP_ERROR_VAE_WORLD_NOT_CONNECTED 32512
#define NIMCP_ERROR_VAE_WORLD_ENCODE_FAILED 32513
#define NIMCP_ERROR_VAE_WORLD_PREDICT_FAILED 32514
#define NIMCP_ERROR_VAE_WORLD_FUSE_FAILED   32515
#define NIMCP_ERROR_VAE_WORLD_NO_MEMORY     32516

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Modality types for multimodal fusion
 */
typedef enum {
    VAE_WORLD_MOD_VISUAL = 0,
    VAE_WORLD_MOD_AUDITORY,
    VAE_WORLD_MOD_TACTILE,
    VAE_WORLD_MOD_PROPRIOCEPTIVE,
    VAE_WORLD_MOD_VESTIBULAR,
    VAE_WORLD_MOD_OLFACTORY,
    VAE_WORLD_MOD_GUSTATORY,
    VAE_WORLD_MOD_INTEROCEPTIVE,
    VAE_WORLD_MOD_LINGUISTIC,
    VAE_WORLD_MOD_SEMANTIC,
    VAE_WORLD_MOD_COUNT
} vae_world_modality_t;

/**
 * @brief Prediction mode
 */
typedef enum {
    VAE_WORLD_PRED_DETERMINISTIC = 0, /**< Point prediction */
    VAE_WORLD_PRED_PROBABILISTIC,      /**< Distribution prediction */
    VAE_WORLD_PRED_ENSEMBLE,           /**< Ensemble of predictions */
    VAE_WORLD_PRED_HIERARCHICAL        /**< Multi-scale prediction */
} vae_world_pred_mode_t;

/**
 * @brief Fusion strategy
 */
typedef enum {
    VAE_WORLD_FUSE_CONCATENATE = 0,  /**< Concatenate latents */
    VAE_WORLD_FUSE_ATTENTION,         /**< Cross-modal attention */
    VAE_WORLD_FUSE_PRODUCT,           /**< Product of experts */
    VAE_WORLD_FUSE_MIXTURE            /**< Mixture of experts */
} vae_world_fusion_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_WORLD_STATE_DISCONNECTED = 0,
    VAE_WORLD_STATE_CONNECTED,
    VAE_WORLD_STATE_ENCODING,
    VAE_WORLD_STATE_PREDICTING,
    VAE_WORLD_STATE_FUSING,
    VAE_WORLD_STATE_ERROR
} vae_world_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Per-modality configuration
 */
typedef struct {
    vae_world_modality_t modality;
    uint32_t input_dim;
    uint32_t latent_dim;
    float weight;                     /**< Weight in fusion */
    bool enabled;
} vae_world_modality_config_t;

/**
 * @brief Entity representation
 */
typedef struct {
    uint32_t entity_id;
    float* latent;                    /**< Entity latent state */
    uint32_t latent_dim;
    float* velocity;                  /**< Latent velocity (for prediction) */
    float confidence;                 /**< Tracking confidence */
    uint64_t last_update_us;
} vae_world_entity_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_world_modality_config_t modalities[VAE_WORLD_MAX_MODALITIES];
    uint32_t num_modalities;

    vae_world_pred_mode_t pred_mode;
    vae_world_fusion_t fusion_strategy;

    /* Latent dimensions */
    uint32_t fused_latent_dim;        /**< Dimension of fused representation */
    uint32_t entity_latent_dim;       /**< Per-entity latent dimension */

    /* Prediction parameters */
    uint32_t default_horizon;         /**< Default prediction steps */
    float prediction_temperature;     /**< Temperature for probabilistic pred */
    bool use_action_conditioning;     /**< Condition prediction on actions */

    /* Attention parameters */
    uint32_t attention_heads;
    uint32_t attention_dim;

    /* Entity tracking */
    bool enable_entity_tracking;
    uint32_t max_entities;

    bool enable_logging;
} vae_world_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Modality encoding result
 */
typedef struct {
    vae_world_modality_t modality;
    float* latent;
    uint32_t latent_dim;
    float* variance;
    float encoding_quality;
} vae_world_modality_result_t;

/**
 * @brief Multimodal fusion result
 */
typedef struct {
    float* fused_latent;              /**< Unified latent representation */
    uint32_t fused_dim;
    float* attention_weights;         /**< Cross-modal attention [n_mod, n_mod] */
    uint32_t num_modalities;
    float fusion_confidence;
    uint64_t fusion_time_us;
} vae_world_fusion_result_t;

/**
 * @brief Prediction result
 */
typedef struct {
    float** predicted_latents;        /**< [horizon, latent_dim] */
    float** predicted_variances;      /**< Prediction uncertainty */
    uint32_t horizon;
    uint32_t latent_dim;
    float* decoded_prediction;        /**< Final decoded prediction */
    uint32_t decoded_dim;
    float prediction_confidence;
    uint64_t prediction_time_us;
} vae_world_prediction_result_t;

/**
 * @brief Entity tracking result
 */
typedef struct {
    vae_world_entity_t* entities;
    uint32_t num_entities;
    float* entity_interactions;       /**< Entity interaction matrix */
    uint64_t tracking_time_us;
} vae_world_entity_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_encodes;
    uint64_t total_predictions;
    uint64_t total_fusions;
    float avg_prediction_confidence;
    float avg_fusion_confidence;
    float per_modality_usage[VAE_WORLD_MOD_COUNT];
    uint32_t active_entities;
    uint64_t creation_time_us;
    uint64_t last_operation_us;
} vae_world_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_world_bridge {
    vae_world_bridge_config_t config;
    vae_system_t* vae;
    void* world_model;                /**< JEPA world model */
    vae_world_bridge_state_t state;
    bool is_initialized;

    /* Per-modality state */
    float* modality_latents[VAE_WORLD_MAX_MODALITIES];
    bool modality_valid[VAE_WORLD_MAX_MODALITIES];

    /* Fused state */
    float* fused_latent;
    float* attention_weights;

    /* Entity tracking */
    vae_world_entity_t* entities;
    uint32_t num_entities;

    /* Working buffers */
    float* encode_buffer;
    float* predict_buffer;
    float* fuse_buffer;

    /* Statistics */
    vae_world_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_world_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_world_bridge_default_config(vae_world_bridge_config_t* config);
vae_world_bridge_t* vae_world_bridge_create(const vae_world_bridge_config_t* config);
void vae_world_bridge_destroy(vae_world_bridge_t* bridge);
int vae_world_bridge_connect_vae(vae_world_bridge_t* bridge, vae_system_t* vae);
int vae_world_bridge_connect_world_model(vae_world_bridge_t* bridge, void* world_model);
int vae_world_bridge_disconnect(vae_world_bridge_t* bridge);
bool vae_world_bridge_is_connected(const vae_world_bridge_t* bridge);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int vae_world_encode_modality(vae_world_bridge_t* bridge,
                               vae_world_modality_t modality,
                               const float* input, uint32_t input_dim,
                               vae_world_modality_result_t* result);

int vae_world_encode_multimodal(vae_world_bridge_t* bridge,
                                 const float** inputs,
                                 const uint32_t* input_dims,
                                 const vae_world_modality_t* modalities,
                                 uint32_t num_modalities,
                                 vae_world_fusion_result_t* result);

/* ============================================================================
 * Fusion API
 * ============================================================================ */

int vae_world_fuse(vae_world_bridge_t* bridge,
                    vae_world_fusion_result_t* result);

int vae_world_fuse_with_attention(vae_world_bridge_t* bridge,
                                   const float* query, uint32_t query_dim,
                                   vae_world_fusion_result_t* result);

int vae_world_compute_cross_modal_attention(vae_world_bridge_t* bridge,
                                             float* attention_weights);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int vae_world_predict(vae_world_bridge_t* bridge,
                       uint32_t horizon,
                       vae_world_prediction_result_t* result);

int vae_world_predict_with_action(vae_world_bridge_t* bridge,
                                   const float* action, uint32_t action_dim,
                                   uint32_t horizon,
                                   vae_world_prediction_result_t* result);

int vae_world_predict_from_latent(vae_world_bridge_t* bridge,
                                   const float* latent, uint32_t latent_dim,
                                   uint32_t horizon,
                                   vae_world_prediction_result_t* result);

/* ============================================================================
 * Entity Tracking API
 * ============================================================================ */

int vae_world_track_entities(vae_world_bridge_t* bridge,
                              const float* observation, uint32_t obs_dim,
                              vae_world_entity_result_t* result);

int vae_world_get_entity(const vae_world_bridge_t* bridge,
                          uint32_t entity_id,
                          vae_world_entity_t* entity);

int vae_world_predict_entity(vae_world_bridge_t* bridge,
                              uint32_t entity_id,
                              uint32_t horizon,
                              float* predicted_latent);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_world_bridge_state_t vae_world_bridge_get_state(const vae_world_bridge_t* bridge);
int vae_world_bridge_get_stats(const vae_world_bridge_t* bridge,
                                vae_world_bridge_stats_t* stats);
int vae_world_get_fused_latent(const vae_world_bridge_t* bridge,
                                float* latent, uint32_t* dim);
const char* vae_world_modality_to_string(vae_world_modality_t modality);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_world_modality_result_free(vae_world_modality_result_t* result);
void vae_world_fusion_result_free(vae_world_fusion_result_t* result);
void vae_world_prediction_result_free(vae_world_prediction_result_t* result);
void vae_world_entity_result_free(vae_world_entity_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_WORLD_MODEL_BRIDGE_H */
