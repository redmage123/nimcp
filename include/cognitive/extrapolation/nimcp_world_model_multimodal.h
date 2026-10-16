/**
 * @file nimcp_world_model_multimodal.h
 * @brief Multi-modal world model for long-horizon prediction
 *
 * WHAT: Integrates multiple sensory modalities into unified world representation
 * WHY:  Enable coherent predictions across visual, auditory, tactile domains
 * HOW:  Hierarchical predictive coding with cross-modal attention
 *
 * INTEGRATION POINTS:
 * - Bio-async messaging for inter-modal communication
 * - Immune system for anomaly detection in predictions
 * - Thalamic gating for attention allocation
 */

#ifndef NIMCP_WORLD_MODEL_MULTIMODAL_H
#define NIMCP_WORLD_MODEL_MULTIMODAL_H

#include "nimcp.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define WM_MAX_MODALITIES       16
#define WM_MAX_PREDICTION_STEPS 100
#define WM_MAX_ENTITIES         1024
#define WM_LATENT_DIM           256
#define WM_CONTEXT_SIZE         512

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

typedef enum {
    WM_OK = 0,
    WM_ERR_NULL_PTR,
    WM_ERR_NOT_INITIALIZED,
    WM_ERR_INVALID_MODALITY,
    WM_ERR_PREDICTION_FAILED,
    WM_ERR_FUSION_FAILED,
    WM_ERR_MEMORY_ALLOC,
    WM_ERR_CAPACITY_EXCEEDED,
    WM_ERR_INVALID_HORIZON,
    WM_ERR_MODALITY_MISMATCH
} wm_error_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

typedef enum {
    WM_MODALITY_VISUAL = 0,
    WM_MODALITY_AUDITORY,
    WM_MODALITY_TACTILE,
    WM_MODALITY_PROPRIOCEPTIVE,
    WM_MODALITY_OLFACTORY,
    WM_MODALITY_GUSTATORY,
    WM_MODALITY_VESTIBULAR,
    WM_MODALITY_INTEROCEPTIVE,
    WM_MODALITY_LINGUISTIC,
    WM_MODALITY_SEMANTIC,
    WM_MODALITY_COUNT
} wm_modality_t;

typedef enum {
    WM_FUSION_EARLY,        /* Fuse at feature level */
    WM_FUSION_LATE,         /* Fuse at decision level */
    WM_FUSION_HIERARCHICAL, /* Multi-level fusion */
    WM_FUSION_ATTENTION     /* Attention-based fusion */
} wm_fusion_type_t;

typedef enum {
    WM_PRED_MODE_DETERMINISTIC,
    WM_PRED_MODE_PROBABILISTIC,
    WM_PRED_MODE_ENSEMBLE,
    WM_PRED_MODE_HIERARCHICAL
} wm_prediction_mode_t;

typedef enum {
    WM_STATUS_IDLE,
    WM_STATUS_PROCESSING,
    WM_STATUS_PREDICTING,
    WM_STATUS_FUSING,
    WM_STATUS_ERROR
} wm_status_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Modality-specific input
 */
typedef struct {
    wm_modality_t modality;
    float* features;
    uint32_t feature_dim;
    float confidence;
    uint64_t timestamp;
    float* attention_weights;
} wm_modality_input_t;

/**
 * @brief Entity representation in world model
 */
typedef struct {
    uint32_t entity_id;
    float position[3];
    float velocity[3];
    float* latent_state;
    uint32_t latent_dim;
    float existence_prob;
    uint64_t last_observed;
    uint32_t modality_mask;   /* Which modalities observed this */
} wm_entity_t;

/**
 * @brief Prediction result
 */
typedef struct {
    uint32_t horizon_steps;
    float* predicted_states;
    float* uncertainties;
    uint32_t state_dim;
    float* entity_predictions;
    uint32_t num_entities;
    float prediction_confidence;
    float surprise;           /* Prediction error */
} wm_prediction_t;

/**
 * @brief Cross-modal attention state
 */
typedef struct {
    float attention_matrix[WM_MODALITY_COUNT][WM_MODALITY_COUNT];
    float modality_weights[WM_MODALITY_COUNT];
    float coherence_score;
    uint32_t dominant_modality;
} wm_cross_modal_attention_t;

/**
 * @brief World model configuration
 */
typedef struct {
    uint32_t latent_dim;
    uint32_t context_size;
    uint32_t max_entities;
    uint32_t max_prediction_steps;
    wm_fusion_type_t fusion_type;
    wm_prediction_mode_t prediction_mode;
    float learning_rate;
    float prediction_decay;
    float entity_persistence;
    bool enable_bio_async;
    bool enable_immune;
    bool enable_logging;
} wm_config_t;

/**
 * @brief World model statistics
 */
typedef struct {
    uint64_t inputs_processed;
    uint64_t predictions_made;
    uint64_t fusion_operations;
    float mean_prediction_error;
    float mean_coherence;
    uint32_t active_entities;
    uint64_t entity_births;
    uint64_t entity_deaths;
} wm_stats_t;

/**
 * @brief Multi-modal world model instance
 */
typedef struct nimcp_world_model {
    wm_config_t config;
    bool initialized;
    wm_status_t status;
    wm_error_t last_error;

    /* Modality encoders */
    float* modality_encoders[WM_MODALITY_COUNT];
    uint32_t encoder_dims[WM_MODALITY_COUNT];
    bool modality_active[WM_MODALITY_COUNT];

    /* Latent world state */
    float* global_state;
    uint32_t global_state_dim;
    float* context_buffer;
    uint32_t context_pos;

    /* Entity tracking */
    wm_entity_t* entities;
    uint32_t num_entities;
    uint32_t entity_capacity;

    /* Cross-modal attention */
    wm_cross_modal_attention_t attention;

    /* Prediction state */
    float* prediction_buffer;
    uint32_t prediction_horizon;

    /* Statistics */
    wm_stats_t stats;

    /* Integration */
    void* bio_async_ctx;
    void* immune_ctx;
} nimcp_world_model_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
NIMCP_EXPORT wm_config_t wm_default_config(void);

/**
 * @brief Create world model instance
 */
NIMCP_EXPORT nimcp_world_model_t* wm_create(const wm_config_t* config);

/**
 * @brief Initialize world model
 */
NIMCP_EXPORT wm_error_t wm_init(nimcp_world_model_t* wm);

/**
 * @brief Reset world model state
 */
NIMCP_EXPORT wm_error_t wm_reset(nimcp_world_model_t* wm);

/**
 * @brief Destroy world model
 */
NIMCP_EXPORT void wm_destroy(nimcp_world_model_t* wm);

/*=============================================================================
 * MODALITY API
 *===========================================================================*/

/**
 * @brief Process input from a specific modality
 */
NIMCP_EXPORT wm_error_t wm_process_modality(
    nimcp_world_model_t* wm,
    const wm_modality_input_t* input);

/**
 * @brief Process multiple modalities simultaneously
 */
NIMCP_EXPORT wm_error_t wm_process_multimodal(
    nimcp_world_model_t* wm,
    const wm_modality_input_t* inputs,
    uint32_t num_inputs);

/**
 * @brief Enable/disable a modality
 */
NIMCP_EXPORT wm_error_t wm_set_modality_active(
    nimcp_world_model_t* wm,
    wm_modality_t modality,
    bool active);

/**
 * @brief Get modality encoding
 */
NIMCP_EXPORT wm_error_t wm_get_modality_encoding(
    nimcp_world_model_t* wm,
    wm_modality_t modality,
    float* encoding,
    uint32_t* dim);

/*=============================================================================
 * FUSION API
 *===========================================================================*/

/**
 * @brief Fuse all active modalities
 */
NIMCP_EXPORT wm_error_t wm_fuse_modalities(nimcp_world_model_t* wm);

/**
 * @brief Get cross-modal attention state
 */
NIMCP_EXPORT wm_error_t wm_get_attention(
    nimcp_world_model_t* wm,
    wm_cross_modal_attention_t* attention);

/**
 * @brief Set fusion weights manually
 */
NIMCP_EXPORT wm_error_t wm_set_fusion_weights(
    nimcp_world_model_t* wm,
    const float* weights,
    uint32_t num_weights);

/*=============================================================================
 * PREDICTION API
 *===========================================================================*/

/**
 * @brief Predict future world state
 */
NIMCP_EXPORT wm_error_t wm_predict(
    nimcp_world_model_t* wm,
    uint32_t horizon_steps,
    wm_prediction_t* prediction);

/**
 * @brief Predict specific entity trajectory
 */
NIMCP_EXPORT wm_error_t wm_predict_entity(
    nimcp_world_model_t* wm,
    uint32_t entity_id,
    uint32_t horizon_steps,
    float* trajectory,
    float* confidence);

/**
 * @brief Update model with prediction error
 */
NIMCP_EXPORT wm_error_t wm_update_prediction_error(
    nimcp_world_model_t* wm,
    const float* actual_state,
    uint32_t state_dim);

/*=============================================================================
 * ENTITY API
 *===========================================================================*/

/**
 * @brief Add entity to world model
 */
NIMCP_EXPORT wm_error_t wm_add_entity(
    nimcp_world_model_t* wm,
    const wm_entity_t* entity,
    uint32_t* entity_id);

/**
 * @brief Update entity state
 */
NIMCP_EXPORT wm_error_t wm_update_entity(
    nimcp_world_model_t* wm,
    uint32_t entity_id,
    const wm_entity_t* update);

/**
 * @brief Get entity by ID
 */
NIMCP_EXPORT wm_error_t wm_get_entity(
    nimcp_world_model_t* wm,
    uint32_t entity_id,
    wm_entity_t* entity);

/**
 * @brief Remove entity
 */
NIMCP_EXPORT wm_error_t wm_remove_entity(
    nimcp_world_model_t* wm,
    uint32_t entity_id);

/**
 * @brief Get all active entities
 */
NIMCP_EXPORT wm_error_t wm_get_entities(
    nimcp_world_model_t* wm,
    wm_entity_t* entities,
    uint32_t* count);

/*=============================================================================
 * STATE API
 *===========================================================================*/

/**
 * @brief Get global world state
 */
NIMCP_EXPORT wm_error_t wm_get_global_state(
    nimcp_world_model_t* wm,
    float* state,
    uint32_t* dim);

/**
 * @brief Update world model
 */
NIMCP_EXPORT wm_error_t wm_update(nimcp_world_model_t* wm, float dt_ms);

/**
 * @brief Get statistics
 */
NIMCP_EXPORT wm_error_t wm_get_stats(
    nimcp_world_model_t* wm,
    wm_stats_t* stats);

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

/**
 * @brief Get status
 */
NIMCP_EXPORT wm_status_t wm_get_status(nimcp_world_model_t* wm);

/**
 * @brief Get last error
 */
NIMCP_EXPORT wm_error_t wm_get_last_error(nimcp_world_model_t* wm);

/**
 * @brief Error to string
 */
NIMCP_EXPORT const char* wm_error_string(wm_error_t error);

/**
 * @brief Status to string
 */
NIMCP_EXPORT const char* wm_status_string(wm_status_t status);

/**
 * @brief Modality to string
 */
NIMCP_EXPORT const char* wm_modality_string(wm_modality_t modality);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORLD_MODEL_MULTIMODAL_H */
