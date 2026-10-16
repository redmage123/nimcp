//=============================================================================
// nimcp_creative_training_bridge.h - Creative Model Training Bridge
//=============================================================================
/**
 * @file nimcp_creative_training_bridge.h
 * @brief Bridge for training and fine-tuning creative models
 *
 * WHAT: Interface for training/fine-tuning creative AI models
 * WHY:  Enable learning from new artistic styles and preferences
 * HOW:  Connect to training pipeline with creative-specific workflows
 *
 * TRAINING TYPES:
 * - Style learning: Learn new artistic styles
 * - Fine-tuning: Adapt models to specific domains
 * - LoRA training: Lightweight adapter training
 * - Preference learning: Learn user preferences
 * - Feedback incorporation: Learn from critiques
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_TRAINING_BRIDGE_H
#define NIMCP_CREATIVE_TRAINING_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Training Types
//=============================================================================

/**
 * @brief Training task types
 */
typedef enum {
    TRAIN_TASK_STYLE_LEARNING = 0, /**< Learn new style from examples */
    TRAIN_TASK_FINE_TUNE,          /**< Full fine-tuning */
    TRAIN_TASK_LORA,               /**< LoRA adapter training */
    TRAIN_TASK_DREAMBOOTH,         /**< DreamBooth personalization */
    TRAIN_TASK_TEXTUAL_INVERSION,  /**< Textual inversion embedding */
    TRAIN_TASK_PREFERENCE,         /**< Learn preferences from feedback */
    TRAIN_TASK_RLHF                /**< Reinforcement from human feedback */
} training_task_t;

/**
 * @brief Training status
 */
typedef enum {
    TRAIN_STATUS_IDLE = 0,         /**< Not training */
    TRAIN_STATUS_PREPARING,        /**< Preparing data */
    TRAIN_STATUS_TRAINING,         /**< Training in progress */
    TRAIN_STATUS_VALIDATING,       /**< Validating results */
    TRAIN_STATUS_SAVING,           /**< Saving model */
    TRAIN_STATUS_COMPLETE,         /**< Training complete */
    TRAIN_STATUS_FAILED,           /**< Training failed */
    TRAIN_STATUS_CANCELLED         /**< Training cancelled */
} training_status_t;

/**
 * @brief Training progress info
 */
typedef struct {
    training_status_t status;      /**< Current status */
    uint32_t epoch;                /**< Current epoch */
    uint32_t total_epochs;         /**< Total epochs */
    uint32_t step;                 /**< Current step */
    uint32_t total_steps;          /**< Total steps */
    float loss;                    /**< Current loss */
    float best_loss;               /**< Best loss so far */
    float learning_rate;           /**< Current learning rate */
    float progress;                /**< [0-1] Overall progress */
    float elapsed_seconds;         /**< Elapsed time */
    float estimated_remaining;     /**< Estimated remaining time */
    char current_phase[64];        /**< Current phase description */
} training_progress_t;

//=============================================================================
// Training Data Types
//=============================================================================

/**
 * @brief Training example (image + caption)
 */
typedef struct {
    visual_image_t image;          /**< Training image */
    char caption[1024];            /**< Image caption */
    float quality_score;           /**< Quality score (for weighting) */
    char style_tags[256];          /**< Style tags */
} image_training_example_t;

/**
 * @brief Preference pair (for RLHF)
 */
typedef struct {
    visual_image_t preferred;      /**< Preferred image */
    visual_image_t rejected;       /**< Rejected image */
    char prompt[512];              /**< Original prompt */
    float preference_strength;     /**< How much more preferred */
} preference_pair_t;

/**
 * @brief Training dataset
 */
typedef struct {
    image_training_example_t* examples;
    uint32_t num_examples;
    preference_pair_t* preferences;
    uint32_t num_preferences;
    char dataset_name[128];
} training_dataset_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Training hyperparameters
 */
typedef struct {
    uint32_t num_epochs;           /**< Number of epochs */
    uint32_t batch_size;           /**< Batch size */
    float learning_rate;           /**< Initial learning rate */
    float weight_decay;            /**< Weight decay */
    float warmup_ratio;            /**< LR warmup ratio */
    char lr_scheduler[32];         /**< LR scheduler type */
    float gradient_clip;           /**< Gradient clipping */
    bool mixed_precision;          /**< Use mixed precision */
    uint32_t save_every_n_steps;   /**< Save checkpoint interval */
} training_hyperparams_t;

/**
 * @brief LoRA-specific config
 */
typedef struct {
    uint32_t rank;                 /**< LoRA rank (4, 8, 16, 32) */
    float alpha;                   /**< LoRA alpha */
    float dropout;                 /**< LoRA dropout */
    char target_modules[256];      /**< Target modules (comma-sep) */
} lora_config_t;

/**
 * @brief Training bridge configuration
 */
typedef struct {
    /* Hyperparameters */
    training_hyperparams_t hyperparams;

    /* LoRA config (if applicable) */
    lora_config_t lora;

    /* Device */
    bool use_gpu;
    int32_t gpu_device_id;
    uint64_t max_vram;

    /* Paths */
    char output_dir[512];          /**< Output directory */
    char checkpoint_dir[512];      /**< Checkpoint directory */
    char log_dir[512];             /**< Logging directory */

    /* Validation */
    float validation_split;        /**< Validation split ratio */
    uint32_t validate_every_n_steps;

    /* Early stopping */
    bool enable_early_stopping;
    uint32_t patience;
    float min_delta;

    /* Callbacks */
    void (*progress_callback)(const training_progress_t*, void*);
    void* callback_context;
} creative_training_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_training_bridge_config_defaults(
    creative_training_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Creative training bridge
 */
struct creative_training_bridge {
    creative_training_bridge_config_t config;

    /* Training state */
    training_status_t status;
    training_task_t current_task;
    training_progress_t progress;

    /* Dataset */
    training_dataset_t* current_dataset;

    /* Models being trained */
    void* model;
    void* optimizer;
    void* lr_scheduler;

    /* Checkpointing */
    char best_checkpoint_path[512];
    float best_metric;

    /* Integration */
    void* brain_training_ctx;

    /* Statistics */
    uint64_t total_training_runs;
    float avg_training_time_hours;
};

/** @brief Typedef for creative_training_bridge */
typedef struct creative_training_bridge creative_training_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create training bridge
 *
 * @param config Configuration
 * @return Bridge or NULL on error
 */
creative_training_bridge_t* creative_training_bridge_create(
    const creative_training_bridge_config_t* config);

/**
 * @brief Destroy training bridge
 *
 * @param bridge Bridge to destroy
 */
void creative_training_bridge_destroy(creative_training_bridge_t* bridge);

//=============================================================================
// Dataset API
//=============================================================================

/**
 * @brief Create dataset
 *
 * @param name Dataset name
 * @return Dataset or NULL on error
 */
training_dataset_t* creative_training_create_dataset(const char* name);

/**
 * @brief Add example to dataset
 *
 * @param dataset Dataset
 * @param image Training image
 * @param caption Caption
 * @param quality Quality score
 * @param tags Style tags
 * @return 0 on success, -1 on error
 */
int creative_training_add_example(training_dataset_t* dataset,
                                   const visual_image_t* image,
                                   const char* caption,
                                   float quality,
                                   const char* tags);

/**
 * @brief Add preference pair
 *
 * @param dataset Dataset
 * @param preferred Preferred image
 * @param rejected Rejected image
 * @param prompt Original prompt
 * @param strength Preference strength
 * @return 0 on success, -1 on error
 */
int creative_training_add_preference(training_dataset_t* dataset,
                                      const visual_image_t* preferred,
                                      const visual_image_t* rejected,
                                      const char* prompt,
                                      float strength);

/**
 * @brief Load dataset from directory
 *
 * @param dir Directory path
 * @return Dataset or NULL on error
 */
training_dataset_t* creative_training_load_dataset(const char* dir);

/**
 * @brief Free dataset
 *
 * @param dataset Dataset to free
 */
void training_dataset_free(training_dataset_t* dataset);

//=============================================================================
// Training API
//=============================================================================

/**
 * @brief Start style learning
 *
 * @param bridge Bridge
 * @param dataset Training dataset
 * @param style_name Name for learned style
 * @return 0 on success, -1 on error
 */
int creative_training_learn_style(creative_training_bridge_t* bridge,
                                   const training_dataset_t* dataset,
                                   const char* style_name);

/**
 * @brief Start LoRA training
 *
 * @param bridge Bridge
 * @param dataset Training dataset
 * @param base_model Path to base model
 * @param output_name Output LoRA name
 * @return 0 on success, -1 on error
 */
int creative_training_train_lora(creative_training_bridge_t* bridge,
                                  const training_dataset_t* dataset,
                                  const char* base_model,
                                  const char* output_name);

/**
 * @brief Start DreamBooth training
 *
 * @param bridge Bridge
 * @param dataset Training dataset
 * @param instance_prompt Instance prompt (e.g., "photo of sks person")
 * @param class_prompt Class prompt (e.g., "photo of person")
 * @return 0 on success, -1 on error
 */
int creative_training_dreambooth(creative_training_bridge_t* bridge,
                                  const training_dataset_t* dataset,
                                  const char* instance_prompt,
                                  const char* class_prompt);

/**
 * @brief Start preference learning (RLHF)
 *
 * @param bridge Bridge
 * @param dataset Dataset with preferences
 * @param reward_model Path to reward model (or NULL to train one)
 * @return 0 on success, -1 on error
 */
int creative_training_learn_preferences(creative_training_bridge_t* bridge,
                                         const training_dataset_t* dataset,
                                         const char* reward_model);

//=============================================================================
// Control API
//=============================================================================

/**
 * @brief Get training progress
 *
 * @param bridge Bridge
 * @param progress Output progress
 * @return 0 on success, -1 on error
 */
int creative_training_get_progress(const creative_training_bridge_t* bridge,
                                    training_progress_t* progress);

/**
 * @brief Pause training
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int creative_training_pause(creative_training_bridge_t* bridge);

/**
 * @brief Resume training
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int creative_training_resume(creative_training_bridge_t* bridge);

/**
 * @brief Cancel training
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int creative_training_cancel(creative_training_bridge_t* bridge);

/**
 * @brief Save checkpoint
 *
 * @param bridge Bridge
 * @param path Checkpoint path
 * @return 0 on success, -1 on error
 */
int creative_training_save_checkpoint(creative_training_bridge_t* bridge,
                                       const char* path);

/**
 * @brief Load checkpoint
 *
 * @param bridge Bridge
 * @param path Checkpoint path
 * @return 0 on success, -1 on error
 */
int creative_training_load_checkpoint(creative_training_bridge_t* bridge,
                                       const char* path);

//=============================================================================
// Output API
//=============================================================================

/**
 * @brief Export trained style embedding
 *
 * @param bridge Bridge
 * @param style Output style embedding
 * @return 0 on success, -1 on error
 */
int creative_training_export_style(creative_training_bridge_t* bridge,
                                    style_embedding_t* style);

/**
 * @brief Export trained LoRA
 *
 * @param bridge Bridge
 * @param output_path Output file path
 * @return 0 on success, -1 on error
 */
int creative_training_export_lora(creative_training_bridge_t* bridge,
                                   const char* output_path);

//=============================================================================
// Feedback API
//=============================================================================

/**
 * @brief Submit feedback for generated content
 *
 * @param bridge Bridge
 * @param content Generated content
 * @param modality Content modality
 * @param rating Rating (1-5)
 * @param feedback Text feedback
 * @return 0 on success, -1 on error
 */
int creative_training_submit_feedback(creative_training_bridge_t* bridge,
                                       const void* content,
                                       art_modality_t modality,
                                       uint8_t rating,
                                       const char* feedback);

/**
 * @brief Apply accumulated feedback
 *
 * @param bridge Bridge
 * @param min_feedback_count Minimum feedback count to apply
 * @return 0 on success, -1 on error
 */
int creative_training_apply_feedback(creative_training_bridge_t* bridge,
                                      uint32_t min_feedback_count);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get task name
 *
 * @param task Task type
 * @return Name string
 */
const char* creative_training_task_name(training_task_t task);

/**
 * @brief Get status name
 *
 * @param status Status
 * @return Name string
 */
const char* creative_training_status_name(training_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_TRAINING_BRIDGE_H */
