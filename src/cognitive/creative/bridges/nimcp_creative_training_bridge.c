//=============================================================================
// nimcp_creative_training_bridge.c - Creative Model Training Bridge
//=============================================================================
/**
 * @file nimcp_creative_training_bridge.c
 * @brief Bridge for training and fine-tuning creative models
 *
 * Implementation of training interface for style learning, LoRA training,
 * DreamBooth personalization, and preference learning from feedback.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/bridges/nimcp_creative_training_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(creative_training_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Thread-local error handling
//=============================================================================

static _Thread_local char g_training_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void set_training_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_training_error, sizeof(g_training_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Internal: Task and status names
//=============================================================================

static const char* g_task_names[] = {
    [TRAIN_TASK_STYLE_LEARNING]    = "style_learning",
    [TRAIN_TASK_FINE_TUNE]         = "fine_tune",
    [TRAIN_TASK_LORA]              = "lora",
    [TRAIN_TASK_DREAMBOOTH]        = "dreambooth",
    [TRAIN_TASK_TEXTUAL_INVERSION] = "textual_inversion",
    [TRAIN_TASK_PREFERENCE]        = "preference_learning",
    [TRAIN_TASK_RLHF]              = "rlhf"
};

static const char* g_status_names[] = {
    [TRAIN_STATUS_IDLE]       = "idle",
    [TRAIN_STATUS_PREPARING]  = "preparing",
    [TRAIN_STATUS_TRAINING]   = "training",
    [TRAIN_STATUS_VALIDATING] = "validating",
    [TRAIN_STATUS_SAVING]     = "saving",
    [TRAIN_STATUS_COMPLETE]   = "complete",
    [TRAIN_STATUS_FAILED]     = "failed",
    [TRAIN_STATUS_CANCELLED]  = "cancelled"
};

//=============================================================================
// Internal: Time helper
//=============================================================================

static float get_elapsed_seconds(struct timespec* start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (float)(now.tv_sec - start->tv_sec) +
           (float)(now.tv_nsec - start->tv_nsec) / 1e9f;
}

//=============================================================================
// Configuration defaults
//=============================================================================

void creative_training_bridge_config_defaults(
    creative_training_bridge_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* Hyperparameters - sensible defaults for fine-tuning */
    config->hyperparams.num_epochs = 10;
    config->hyperparams.batch_size = 4;
    config->hyperparams.learning_rate = 1e-5f;
    config->hyperparams.weight_decay = 0.01f;
    config->hyperparams.warmup_ratio = 0.1f;
    strncpy(config->hyperparams.lr_scheduler, "cosine",
            sizeof(config->hyperparams.lr_scheduler) - 1);
    config->hyperparams.gradient_clip = NIMCP_GRADIENT_CLIP_DEFAULT;
    config->hyperparams.mixed_precision = true;
    config->hyperparams.save_every_n_steps = 500;

    /* LoRA config */
    config->lora.rank = 8;
    config->lora.alpha = 16.0f;
    config->lora.dropout = 0.05f;
    strncpy(config->lora.target_modules, "q_proj,v_proj,k_proj,out_proj",
            sizeof(config->lora.target_modules) - 1);

    /* Device */
    config->use_gpu = true;
    config->gpu_device_id = 0;
    config->max_vram = 8ULL * 1024 * 1024 * 1024; /* 8GB */

    /* Paths */
    strncpy(config->output_dir, "./output", sizeof(config->output_dir) - 1);
    strncpy(config->checkpoint_dir, "./checkpoints", sizeof(config->checkpoint_dir) - 1);
    strncpy(config->log_dir, "./logs", sizeof(config->log_dir) - 1);

    /* Validation */
    config->validation_split = 0.1f;
    config->validate_every_n_steps = 100;

    /* Early stopping */
    config->enable_early_stopping = true;
    config->patience = 3;
    config->min_delta = 0.001f;

    /* No callback by default */
    config->progress_callback = NULL;
    config->callback_context = NULL;
}

//=============================================================================
// Lifecycle API
//=============================================================================

creative_training_bridge_t* creative_training_bridge_create(
    const creative_training_bridge_config_t* config) {

    if (!config) {
        set_training_error("NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_elapsed_seconds: config is NULL");
        return NULL;
    }

    creative_training_bridge_t* bridge = nimcp_calloc(1, sizeof(creative_training_bridge_t));
    if (!bridge) {
        set_training_error("Failed to allocate training bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_elapsed_seconds: bridge is NULL");
        return NULL;
    }

    /* Copy config */
    memcpy(&bridge->config, config, sizeof(creative_training_bridge_config_t));

    /* Initialize state */
    bridge->status = TRAIN_STATUS_IDLE;
    bridge->current_task = TRAIN_TASK_STYLE_LEARNING;
    memset(&bridge->progress, 0, sizeof(bridge->progress));
    bridge->progress.status = TRAIN_STATUS_IDLE;

    /* No dataset initially */
    bridge->current_dataset = NULL;

    /* No model/optimizer initially */
    bridge->model = NULL;
    bridge->optimizer = NULL;
    bridge->lr_scheduler = NULL;

    /* Checkpointing */
    memset(bridge->best_checkpoint_path, 0, sizeof(bridge->best_checkpoint_path));
    bridge->best_metric = INFINITY;

    /* Integration */
    bridge->brain_training_ctx = NULL;

    /* Statistics */
    bridge->total_training_runs = 0;
    bridge->avg_training_time_hours = 0.0f;

    return bridge;
}

void creative_training_bridge_destroy(creative_training_bridge_t* bridge) {
    if (!bridge) return;

    /* Cancel any ongoing training */
    if (bridge->status == TRAIN_STATUS_TRAINING ||
        bridge->status == TRAIN_STATUS_PREPARING) {
        creative_training_cancel(bridge);
    }

    /* Free current dataset if owned */
    if (bridge->current_dataset) {
        training_dataset_free(bridge->current_dataset);
        bridge->current_dataset = NULL;
    }

    /* In production, would clean up model/optimizer handles */

    nimcp_free(bridge);
    bridge = NULL;
}

//=============================================================================
// Dataset API
//=============================================================================

training_dataset_t* creative_training_create_dataset(const char* name) {
    training_dataset_t* dataset = nimcp_calloc(1, sizeof(training_dataset_t));
    if (!dataset) {
        set_training_error("Failed to allocate dataset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_training_create_dataset: dataset is NULL");
        return NULL;
    }

    if (name) {
        strncpy(dataset->dataset_name, name, sizeof(dataset->dataset_name) - 1);
    }

    dataset->examples = NULL;
    dataset->num_examples = 0;
    dataset->preferences = NULL;
    dataset->num_preferences = 0;

    return dataset;
}

int creative_training_add_example(training_dataset_t* dataset,
                                   const visual_image_t* image,
                                   const char* caption,
                                   float quality,
                                   const char* tags) {
    if (!dataset || !image || !caption) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_create_dataset: required parameter is NULL (dataset, image, caption)");
        return -1;
    }

    /* Expand examples array */
    uint32_t new_count = dataset->num_examples + 1;
    image_training_example_t* new_examples = nimcp_calloc(
        new_count, sizeof(image_training_example_t));
    if (!new_examples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_training_create_dataset: new_examples is NULL");
        return -1;
    }

    /* Copy existing examples */
    if (dataset->examples && dataset->num_examples > 0) {
        memcpy(new_examples, dataset->examples,
               dataset->num_examples * sizeof(image_training_example_t));
        nimcp_free(dataset->examples);
    }

    /* Add new example */
    image_training_example_t* example = &new_examples[new_count - 1];

    /* Copy image data */
    example->image = *image;
    size_t image_data_size = (size_t)image->width * image->height * image->channels;
    if (image->pixels && image_data_size > 0) {
        example->image.pixels = nimcp_calloc(image_data_size, 1);
        if (example->image.pixels) {
            memcpy(example->image.pixels, image->pixels, image_data_size);
            example->image.owns_pixels = true;
        }
    }

    strncpy(example->caption, caption, sizeof(example->caption) - 1);
    example->quality_score = quality;
    if (tags) {
        strncpy(example->style_tags, tags, sizeof(example->style_tags) - 1);
    }

    dataset->examples = new_examples;
    dataset->num_examples = new_count;

    return 0;
}

int creative_training_add_preference(training_dataset_t* dataset,
                                      const visual_image_t* preferred,
                                      const visual_image_t* rejected,
                                      const char* prompt,
                                      float strength) {
    if (!dataset || !preferred || !rejected || !prompt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_create_dataset: required parameter is NULL (dataset, preferred, rejected, prompt)");
        return -1;
    }

    /* Expand preferences array */
    uint32_t new_count = dataset->num_preferences + 1;
    preference_pair_t* new_preferences = nimcp_calloc(
        new_count, sizeof(preference_pair_t));
    if (!new_preferences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_training_create_dataset: new_preferences is NULL");
        return -1;
    }

    /* Copy existing preferences */
    if (dataset->preferences && dataset->num_preferences > 0) {
        memcpy(new_preferences, dataset->preferences,
               dataset->num_preferences * sizeof(preference_pair_t));
        nimcp_free(dataset->preferences);
    }

    /* Add new preference pair */
    preference_pair_t* pref = &new_preferences[new_count - 1];

    /* Copy preferred image */
    pref->preferred = *preferred;
    size_t pref_data_size = (size_t)preferred->width * preferred->height * preferred->channels;
    if (preferred->pixels && pref_data_size > 0) {
        pref->preferred.pixels = nimcp_calloc(pref_data_size, 1);
        if (pref->preferred.pixels) {
            memcpy(pref->preferred.pixels, preferred->pixels, pref_data_size);
            pref->preferred.owns_pixels = true;
        }
    }

    /* Copy rejected image */
    pref->rejected = *rejected;
    size_t rej_data_size = (size_t)rejected->width * rejected->height * rejected->channels;
    if (rejected->pixels && rej_data_size > 0) {
        pref->rejected.pixels = nimcp_calloc(rej_data_size, 1);
        if (pref->rejected.pixels) {
            memcpy(pref->rejected.pixels, rejected->pixels, rej_data_size);
            pref->rejected.owns_pixels = true;
        }
    }

    strncpy(pref->prompt, prompt, sizeof(pref->prompt) - 1);
    pref->preference_strength = strength;

    dataset->preferences = new_preferences;
    dataset->num_preferences = new_count;

    return 0;
}

training_dataset_t* creative_training_load_dataset(const char* dir) {
    if (!dir) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_load_dataset: dir is NULL");
        return NULL;
    }

    /* In production, would scan directory for images and captions */
    /* For now, create empty dataset */
    training_dataset_t* dataset = creative_training_create_dataset(dir);
    if (!dataset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_training_load_dataset: dataset is NULL");
        return NULL;
    }

    /* Placeholder: would load images from dir/*.{png,jpg} with captions from *.txt */
    set_training_error("Dataset loading not implemented - created empty dataset");

    return dataset;
}

void training_dataset_free(training_dataset_t* dataset) {
    if (!dataset) return;

    /* Free example images */
    if (dataset->examples) {
        for (uint32_t i = 0; i < dataset->num_examples; i++) {
            if (dataset->examples[i].image.pixels && dataset->examples[i].image.owns_pixels) {
                nimcp_free(dataset->examples[i].image.pixels);
            }
        }
        nimcp_free(dataset->examples);
    }

    /* Free preference pair images */
    if (dataset->preferences) {
        for (uint32_t i = 0; i < dataset->num_preferences; i++) {
            if (dataset->preferences[i].preferred.pixels && dataset->preferences[i].preferred.owns_pixels) {
                nimcp_free(dataset->preferences[i].preferred.pixels);
            }
            if (dataset->preferences[i].rejected.pixels && dataset->preferences[i].rejected.owns_pixels) {
                nimcp_free(dataset->preferences[i].rejected.pixels);
            }
        }
        nimcp_free(dataset->preferences);
    }

    nimcp_free(dataset);
    dataset = NULL;
}

//=============================================================================
// Internal: Simulated training loop
//=============================================================================

static void simulate_training_step(creative_training_bridge_t* bridge) {
    /* Simulate a training step with loss decay */
    if (bridge->progress.step == 0) {
        bridge->progress.loss = 2.5f;
        bridge->progress.best_loss = bridge->progress.loss;
    } else {
        /* Exponential decay with noise */
        float decay = expf(-0.001f * (float)bridge->progress.step);
        float noise = ((float)nimcp_tl_rand() / RAND_MAX) * 0.1f - 0.05f;
        bridge->progress.loss = 0.1f + 2.4f * decay + noise;

        if (bridge->progress.loss < bridge->progress.best_loss) {
            bridge->progress.best_loss = bridge->progress.loss;
        }
    }

    bridge->progress.step++;

    /* Update learning rate (cosine decay) */
    float progress_ratio = (float)bridge->progress.step / (float)bridge->progress.total_steps;
    bridge->progress.learning_rate = bridge->config.hyperparams.learning_rate *
                                     (0.5f * (1.0f + cosf(NIMCP_PI_F * progress_ratio)));

    /* Update overall progress */
    bridge->progress.progress = progress_ratio;
}

//=============================================================================
// Training API
//=============================================================================

int creative_training_learn_style(creative_training_bridge_t* bridge,
                                   const training_dataset_t* dataset,
                                   const char* style_name) {
    if (!bridge || !dataset || !style_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "simulate_training_step: required parameter is NULL (bridge, dataset, style_name)");
        return -1;
    }

    if (dataset->num_examples < 5) {
        set_training_error("Style learning requires at least 5 examples");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "simulate_training_step: validation failed");
        return -1;
    }

    if (bridge->status == TRAIN_STATUS_TRAINING) {
        set_training_error("Training already in progress");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "simulate_training_step: validation failed");
        return -1;
    }

    /* Initialize training state */
    bridge->status = TRAIN_STATUS_PREPARING;
    bridge->current_task = TRAIN_TASK_STYLE_LEARNING;
    bridge->progress.status = TRAIN_STATUS_PREPARING;
    strncpy(bridge->progress.current_phase, "Preparing style learning",
            sizeof(bridge->progress.current_phase) - 1);

    /* Calculate training parameters */
    uint32_t steps_per_epoch = (dataset->num_examples + bridge->config.hyperparams.batch_size - 1) /
                               bridge->config.hyperparams.batch_size;
    bridge->progress.total_epochs = bridge->config.hyperparams.num_epochs;
    bridge->progress.total_steps = steps_per_epoch * bridge->config.hyperparams.num_epochs;
    bridge->progress.epoch = 0;
    bridge->progress.step = 0;
    bridge->progress.loss = 0.0f;
    bridge->progress.best_loss = INFINITY;
    bridge->progress.learning_rate = bridge->config.hyperparams.learning_rate;
    bridge->progress.progress = 0.0f;
    bridge->progress.elapsed_seconds = 0.0f;

    /* In production, would:
     * 1. Load pretrained model
     * 2. Create optimizer
     * 3. Set up data loaders
     * 4. Start training loop in background thread
     */

    /* Transition to training */
    bridge->status = TRAIN_STATUS_TRAINING;
    bridge->progress.status = TRAIN_STATUS_TRAINING;
    strncpy(bridge->progress.current_phase, "Learning style embeddings",
            sizeof(bridge->progress.current_phase) - 1);

    /* Simulate a few training steps for demonstration */
    for (int i = 0; i < 10 && bridge->progress.step < bridge->progress.total_steps; i++) {
        simulate_training_step(bridge);
    }

    bridge->total_training_runs++;

    return 0;
}

int creative_training_train_lora(creative_training_bridge_t* bridge,
                                  const training_dataset_t* dataset,
                                  const char* base_model,
                                  const char* output_name) {
    if (!bridge || !dataset || !base_model || !output_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "simulate_training_step: required parameter is NULL (bridge, dataset, base_model, output_name)");
        return -1;
    }

    if (dataset->num_examples < 10) {
        set_training_error("LoRA training requires at least 10 examples");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "simulate_training_step: validation failed");
        return -1;
    }

    if (bridge->status == TRAIN_STATUS_TRAINING) {
        set_training_error("Training already in progress");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "simulate_training_step: validation failed");
        return -1;
    }

    /* Initialize training state */
    bridge->status = TRAIN_STATUS_PREPARING;
    bridge->current_task = TRAIN_TASK_LORA;
    bridge->progress.status = TRAIN_STATUS_PREPARING;
    strncpy(bridge->progress.current_phase, "Preparing LoRA adapters",
            sizeof(bridge->progress.current_phase) - 1);

    /* Calculate steps */
    uint32_t steps_per_epoch = (dataset->num_examples + bridge->config.hyperparams.batch_size - 1) /
                               bridge->config.hyperparams.batch_size;
    bridge->progress.total_epochs = bridge->config.hyperparams.num_epochs;
    bridge->progress.total_steps = steps_per_epoch * bridge->config.hyperparams.num_epochs;
    bridge->progress.epoch = 0;
    bridge->progress.step = 0;
    bridge->progress.best_loss = INFINITY;
    bridge->progress.learning_rate = bridge->config.hyperparams.learning_rate;

    /* In production, would:
     * 1. Load base model
     * 2. Insert LoRA adapters with config (rank, alpha, dropout)
     * 3. Freeze base weights, only train LoRA
     * 4. Start training
     */

    bridge->status = TRAIN_STATUS_TRAINING;
    bridge->progress.status = TRAIN_STATUS_TRAINING;
    snprintf(bridge->progress.current_phase, sizeof(bridge->progress.current_phase),
             "Training LoRA (rank=%u)", bridge->config.lora.rank);

    /* Simulate training steps */
    for (int i = 0; i < 10 && bridge->progress.step < bridge->progress.total_steps; i++) {
        simulate_training_step(bridge);
    }

    bridge->total_training_runs++;

    return 0;
}

int creative_training_dreambooth(creative_training_bridge_t* bridge,
                                  const training_dataset_t* dataset,
                                  const char* instance_prompt,
                                  const char* class_prompt) {
    if (!bridge || !dataset || !instance_prompt || !class_prompt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "simulate_training_step: required parameter is NULL (bridge, dataset, instance_prompt, class_prompt)");
        return -1;
    }

    if (dataset->num_examples < 3) {
        set_training_error("DreamBooth requires at least 3-5 images");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "simulate_training_step: validation failed");
        return -1;
    }

    if (bridge->status == TRAIN_STATUS_TRAINING) {
        set_training_error("Training already in progress");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "simulate_training_step: validation failed");
        return -1;
    }

    bridge->status = TRAIN_STATUS_PREPARING;
    bridge->current_task = TRAIN_TASK_DREAMBOOTH;
    bridge->progress.status = TRAIN_STATUS_PREPARING;
    strncpy(bridge->progress.current_phase, "Preparing DreamBooth",
            sizeof(bridge->progress.current_phase) - 1);

    /* DreamBooth typically uses fewer epochs but more steps */
    bridge->progress.total_epochs = 3;
    bridge->progress.total_steps = 800; /* Typical for DreamBooth */
    bridge->progress.epoch = 0;
    bridge->progress.step = 0;
    bridge->progress.best_loss = INFINITY;
    bridge->progress.learning_rate = 1e-6f; /* Lower LR for DreamBooth */

    bridge->status = TRAIN_STATUS_TRAINING;
    bridge->progress.status = TRAIN_STATUS_TRAINING;
    snprintf(bridge->progress.current_phase, sizeof(bridge->progress.current_phase),
             "DreamBooth: %s", instance_prompt);

    for (int i = 0; i < 10 && bridge->progress.step < bridge->progress.total_steps; i++) {
        simulate_training_step(bridge);
    }

    bridge->total_training_runs++;

    return 0;
}

int creative_training_learn_preferences(creative_training_bridge_t* bridge,
                                         const training_dataset_t* dataset,
                                         const char* reward_model) {
    if (!bridge || !dataset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "simulate_training_step: required parameter is NULL (bridge, dataset)");
        return -1;
    }

    if (dataset->num_preferences < 10) {
        set_training_error("Preference learning requires at least 10 preference pairs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_training_learn_preferences: insufficient preference pairs");
        return -1;
    }

    if (bridge->status == TRAIN_STATUS_TRAINING) {
        set_training_error("Training already in progress");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    (void)reward_model; /* Would use or train reward model */

    bridge->status = TRAIN_STATUS_PREPARING;
    bridge->current_task = TRAIN_TASK_PREFERENCE;
    bridge->progress.status = TRAIN_STATUS_PREPARING;
    strncpy(bridge->progress.current_phase, "Preparing preference learning",
            sizeof(bridge->progress.current_phase) - 1);

    /* Preference learning setup */
    bridge->progress.total_epochs = 5;
    bridge->progress.total_steps = dataset->num_preferences * bridge->progress.total_epochs;
    bridge->progress.epoch = 0;
    bridge->progress.step = 0;
    bridge->progress.best_loss = INFINITY;
    bridge->progress.learning_rate = bridge->config.hyperparams.learning_rate;

    bridge->status = TRAIN_STATUS_TRAINING;
    bridge->progress.status = TRAIN_STATUS_TRAINING;
    strncpy(bridge->progress.current_phase, "Learning from preferences",
            sizeof(bridge->progress.current_phase) - 1);

    for (int i = 0; i < 10 && bridge->progress.step < bridge->progress.total_steps; i++) {
        simulate_training_step(bridge);
    }

    bridge->total_training_runs++;

    return 0;
}

//=============================================================================
// Control API
//=============================================================================

int creative_training_get_progress(const creative_training_bridge_t* bridge,
                                    training_progress_t* progress) {
    if (!bridge || !progress) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, progress)");
        return -1;
    }

    memcpy(progress, &bridge->progress, sizeof(training_progress_t));
    return 0;
}

int creative_training_pause(creative_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_pause: bridge is NULL");
        return -1;
    }

    if (bridge->status != TRAIN_STATUS_TRAINING) {
        set_training_error("Not currently training");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_training_pause: validation failed");
        return -1;
    }

    /* In production, would signal training thread to pause */
    /* For now, just update status */
    bridge->status = TRAIN_STATUS_IDLE; /* Paused state */
    strncpy(bridge->progress.current_phase, "Paused",
            sizeof(bridge->progress.current_phase) - 1);

    return 0;
}

int creative_training_resume(creative_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_resume: bridge is NULL");
        return -1;
    }

    if (bridge->progress.step >= bridge->progress.total_steps) {
        set_training_error("Training already complete");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_training_resume: capacity exceeded");
        return -1;
    }

    bridge->status = TRAIN_STATUS_TRAINING;
    bridge->progress.status = TRAIN_STATUS_TRAINING;
    strncpy(bridge->progress.current_phase, "Resumed training",
            sizeof(bridge->progress.current_phase) - 1);

    return 0;
}

int creative_training_cancel(creative_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_cancel: bridge is NULL");
        return -1;
    }

    /* In production, would signal training thread to stop */
    bridge->status = TRAIN_STATUS_CANCELLED;
    bridge->progress.status = TRAIN_STATUS_CANCELLED;
    strncpy(bridge->progress.current_phase, "Cancelled",
            sizeof(bridge->progress.current_phase) - 1);

    return 0;
}

int creative_training_save_checkpoint(creative_training_bridge_t* bridge,
                                       const char* path) {
    if (!bridge || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_cancel: required parameter is NULL (bridge, path)");
        return -1;
    }

    /* In production, would serialize:
     * - Model weights
     * - Optimizer state
     * - Training progress
     * - Random state
     */

    bridge->status = TRAIN_STATUS_SAVING;
    bridge->progress.status = TRAIN_STATUS_SAVING;
    snprintf(bridge->progress.current_phase, sizeof(bridge->progress.current_phase),
             "Saving checkpoint to %s", path);

    /* Save best checkpoint path if this is best */
    if (bridge->progress.loss < bridge->best_metric) {
        bridge->best_metric = bridge->progress.loss;
        strncpy(bridge->best_checkpoint_path, path,
                sizeof(bridge->best_checkpoint_path) - 1);
    }

    /* Restore status */
    bridge->status = TRAIN_STATUS_TRAINING;
    bridge->progress.status = TRAIN_STATUS_TRAINING;

    return 0;
}

int creative_training_load_checkpoint(creative_training_bridge_t* bridge,
                                       const char* path) {
    if (!bridge || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_cancel: required parameter is NULL (bridge, path)");
        return -1;
    }

    /* In production, would deserialize checkpoint */
    bridge->status = TRAIN_STATUS_PREPARING;
    bridge->progress.status = TRAIN_STATUS_PREPARING;
    snprintf(bridge->progress.current_phase, sizeof(bridge->progress.current_phase),
             "Loading checkpoint from %s", path);

    /* Placeholder: would restore state */
    set_training_error("Checkpoint loading not fully implemented");

    bridge->status = TRAIN_STATUS_IDLE;
    bridge->progress.status = TRAIN_STATUS_IDLE;

    return 0;
}

//=============================================================================
// Output API
//=============================================================================

int creative_training_export_style(creative_training_bridge_t* bridge,
                                    style_embedding_t* style) {
    if (!bridge || !style) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_cancel: required parameter is NULL (bridge, style)");
        return -1;
    }

    if (bridge->current_task != TRAIN_TASK_STYLE_LEARNING) {
        set_training_error("No style learning task");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_training_cancel: validation failed");
        return -1;
    }

    if (bridge->status != TRAIN_STATUS_COMPLETE &&
        bridge->progress.step < bridge->progress.total_steps / 2) {
        set_training_error("Training not sufficiently progressed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_training_cancel: validation failed");
        return -1;
    }

    /* In production, would extract learned style embedding */
    memset(style, 0, sizeof(*style));

    /* Default embedding dimension */
    style->embedding_dim = 512;
    style->embedding = nimcp_calloc(style->embedding_dim, sizeof(float));
    if (!style->embedding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_training_cancel: style->embedding is NULL");
        return -1;
    }

    /* Fill with placeholder values (would be learned in real training) */
    for (uint32_t i = 0; i < style->embedding_dim; i++) {
        style->embedding[i] = ((float)nimcp_tl_rand() / RAND_MAX) * 2.0f - 1.0f;
    }

    strncpy(style->style_name, "Learned style",
            sizeof(style->style_name) - 1);
    style->confidence = bridge->progress.progress;

    return 0;
}

int creative_training_export_lora(creative_training_bridge_t* bridge,
                                   const char* output_path) {
    if (!bridge || !output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_cancel: required parameter is NULL (bridge, output_path)");
        return -1;
    }

    if (bridge->current_task != TRAIN_TASK_LORA) {
        set_training_error("No LoRA training task");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_training_cancel: validation failed");
        return -1;
    }

    if (bridge->status != TRAIN_STATUS_COMPLETE &&
        bridge->progress.step < bridge->progress.total_steps / 2) {
        set_training_error("LoRA training not sufficiently progressed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_training_cancel: validation failed");
        return -1;
    }

    /* In production, would serialize LoRA weights in safetensors format */
    /* Placeholder: would write to output_path */

    set_training_error("LoRA export: would write to %s", output_path);

    return 0;
}

//=============================================================================
// Feedback API
//=============================================================================

typedef struct {
    void* content;
    art_modality_t modality;
    uint8_t rating;
    char feedback[NIMCP_ERROR_BUFFER_LARGE];
} feedback_entry_t;

static feedback_entry_t* g_feedback_buffer = NULL;
static uint32_t g_feedback_count = 0;
static uint32_t g_feedback_capacity = 0;
static nimcp_mutex_t g_feedback_mutex = NIMCP_MUTEX_INITIALIZER;

int creative_training_submit_feedback(creative_training_bridge_t* bridge,
                                       const void* content,
                                       art_modality_t modality,
                                       uint8_t rating,
                                       const char* feedback) {
    if (!bridge || !content || rating < 1 || rating > 5) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_cancel: required parameter is NULL (bridge, content)");
        return -1;
    }

    nimcp_platform_mutex_lock(&g_feedback_mutex);

    /* Expand feedback buffer if needed */
    if (g_feedback_count >= g_feedback_capacity) {
        uint32_t new_capacity = (g_feedback_capacity == 0) ? 16 : g_feedback_capacity * 2;
        feedback_entry_t* new_buffer = nimcp_calloc(new_capacity, sizeof(feedback_entry_t));
        if (!new_buffer) {
            nimcp_platform_mutex_unlock(&g_feedback_mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_training_cancel: new_buffer is NULL");
            return -1;
        }
        if (g_feedback_buffer && g_feedback_count > 0) {
            memcpy(new_buffer, g_feedback_buffer, g_feedback_count * sizeof(feedback_entry_t));
            nimcp_free(g_feedback_buffer);
            g_feedback_buffer = NULL;
        }
        g_feedback_buffer = new_buffer;
        g_feedback_capacity = new_capacity;
    }

    /* Add feedback entry.
     * WARNING: content is stored as a borrowed reference (void* without size).
     * Caller MUST keep the content alive until creative_training_apply_feedback()
     * is called. A proper fix would require adding a content_size parameter to
     * the API so we can deep-copy, but that requires a header change. */
    feedback_entry_t* entry = &g_feedback_buffer[g_feedback_count++];
    entry->content = (void*)content;
    entry->modality = modality;
    entry->rating = rating;
    if (feedback) {
        strncpy(entry->feedback, feedback, sizeof(entry->feedback) - 1);
    }

    nimcp_platform_mutex_unlock(&g_feedback_mutex);
    return 0;
}

int creative_training_apply_feedback(creative_training_bridge_t* bridge,
                                      uint32_t min_feedback_count) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_training_cancel: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(&g_feedback_mutex);

    if (g_feedback_count < min_feedback_count) {
        uint32_t have = g_feedback_count;
        nimcp_platform_mutex_unlock(&g_feedback_mutex);
        set_training_error("Insufficient feedback: have %u, need %u",
                          have, min_feedback_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_training_cancel: validation failed");
        return -1;
    }

    /* In production, would:
     * 1. Convert feedback to preference pairs
     * 2. Train reward model or update generation model
     */

    /* Clear applied feedback */
    g_feedback_count = 0;

    nimcp_platform_mutex_unlock(&g_feedback_mutex);
    return 0;
}

//=============================================================================
// Utility API
//=============================================================================

const char* creative_training_task_name(training_task_t task) {
    if (task >= 0 && task <= TRAIN_TASK_RLHF) {
        return g_task_names[task];
    }
    return "unknown";
}

const char* creative_training_status_name(training_status_t status) {
    if (status >= TRAIN_STATUS_IDLE && status <= TRAIN_STATUS_CANCELLED) {
        return g_status_names[status];
    }
    return "unknown";
}
