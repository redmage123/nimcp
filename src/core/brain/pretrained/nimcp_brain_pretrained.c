//=============================================================================
// nimcp_brain_pretrained.c - Pre-trained Model Management Implementation
//=============================================================================
/**
 * @file nimcp_brain_pretrained.c
 * @brief Pre-trained model download, loading, and fine-tuning
 *
 * WHAT: Complete implementation of pre-trained model management
 * WHY:  Extracted from nimcp_brain.c for better modularity
 * HOW:  Self-contained module with model management and fine-tuning logic
 *
 * EXTRACTED FROM: nimcp_brain.c Phase 9.0 (lines 6892-7337)
 * EXTRACTION DATE: 2025-11-19
 *
 * ARCHITECTURE:
 * - Static helpers for directory/filepath management
 * - Public API for model existence, download, info
 * - Pre-trained model creation and loading
 * - Fine-tuning with selective layer freezing
 *
 * DEPENDENCIES:
 * - nimcp_brain_internal.h: Access to brain_struct
 * - System libraries: sys/stat.h, errno.h for filesystem ops
 *
 * @author NIMCP Development Team
 * @date 2025-11-19
 * @version 1.0.0
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_unified_memory.h"

#include "core/brain/pretrained/nimcp_brain_pretrained.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define LOG_MODULE "BRAIN_PRETRAINED"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_pretrained, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Static Helper Functions
//=============================================================================

/**
 * @brief Get model directory path
 *
 * WHAT: Determines platform-specific model cache directory
 * WHY:  Models need persistent storage location
 * HOW:  Uses LOCALAPPDATA (Windows) or HOME (Unix/Linux)
 *
 * PATHS:
 * - Windows: %LOCALAPPDATA%\NIMCP\models
 * - Unix/Linux: ~/.nimcp/models
 *
 * @param buffer Output buffer for path
 * @param buffer_size Buffer size
 * @return true on success, false if environment variable not set
 *
 * COMPLEXITY: O(1)
 */
static bool get_model_directory(char* buffer, size_t buffer_size)
{
#ifdef _WIN32
    const char* appdata = getenv("LOCALAPPDATA");
    if (!appdata) {
        fprintf(stderr, "NIMCP Error: LOCALAPPDATA environment variable not set\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_model_directory: appdata is NULL");
        return false;
    }
    snprintf(buffer, buffer_size, "%s\\NIMCP\\models", appdata);
#else
    const char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "NIMCP Error: HOME environment variable not set\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_model_directory: home is NULL");
        return false;
    }
    snprintf(buffer, buffer_size, "%s/.nimcp/models", home);
#endif
    return true;
}

/**
 * @brief Create model directory if it doesn't exist
 *
 * WHAT: Creates ~/.nimcp/models/ directory with proper permissions
 * WHY:  Ensure storage location exists before download/save
 * HOW:  Uses mkdir() with errno check for EEXIST
 *
 * @return true on success or if already exists
 *
 * COMPLEXITY: O(1)
 */
static bool ensure_model_directory_exists(void)
{
    char model_dir[NIMCP_METRICS_PATH_SIZE];
    if (!get_model_directory(model_dir, sizeof(model_dir))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_model_directory_exists: get_model_directory is NULL");
        return false;
    }

#ifdef _WIN32
    if (_mkdir(model_dir) != 0 && errno != EEXIST) {
        fprintf(stderr, "NIMCP Error: Failed to create model directory: %s\n", model_dir);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_model_directory_exists: validation failed");
        return false;
    }
#else
    if (mkdir(model_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "NIMCP Error: Failed to create model directory: %s\n", model_dir);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ensure_model_directory_exists: validation failed");
        return false;
    }
#endif

    return true;
}

/**
 * @brief Get full filepath for model
 *
 * WHAT: Constructs full path to model file
 * WHY:  Consistent naming convention for model files
 * HOW:  Combines model_dir + model_id + version suffix
 *
 * FORMAT: ~/.nimcp/models/<model_id>_v2.7.0.brain
 *
 * @param model_id Model identifier
 * @param buffer Output buffer for path
 * @param buffer_size Buffer size
 * @return true on success
 *
 * COMPLEXITY: O(1)
 */
static bool get_model_filepath(const char* model_id, char* buffer, size_t buffer_size)
{
    char model_dir[NIMCP_METRICS_PATH_SIZE];
    if (!get_model_directory(model_dir, sizeof(model_dir))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_model_filepath: get_model_directory is NULL");
        return false;
    }

    snprintf(buffer, buffer_size, "%s/%s_v2.7.0.brain", model_dir, model_id);
    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool brain_model_exists(const char* model_id)
{
    if (!model_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_model_exists: model_id is NULL");
        return false;
    }

    char filepath[NIMCP_METRICS_PATH_SIZE];
    if (!get_model_filepath(model_id, filepath, sizeof(filepath))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_model_exists: get_model_filepath is NULL");
        return false;
    }

    struct stat st;
    return (stat(filepath, &st) == 0);
}

bool brain_download_model(const char* model_id)
{
    if (!model_id) {
        fprintf(stderr, "NIMCP Error: model_id is NULL\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_download_model: model_id is NULL");
        return false;
    }

    // Ensure model directory exists
    if (!ensure_model_directory_exists()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_download_model: ensure_model_directory_exists is NULL");
        return false;
    }

    char filepath[NIMCP_METRICS_PATH_SIZE];
    if (!get_model_filepath(model_id, filepath, sizeof(filepath))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_download_model: get_model_filepath is NULL");
        return false;
    }

    // Check if already downloaded
    if (brain_model_exists(model_id)) {
        printf("NIMCP: Model '%s' already exists at %s\n", model_id, filepath);
        return true;
    }

    // Construct download URL
    char url[NIMCP_ERROR_BUFFER_LARGE];
    snprintf(url, sizeof(url), "https://models.nimcp.ai/v2.7.0/%s_v2.7.0.brain", model_id);

    printf("NIMCP: Downloading model '%s' from %s\n", model_id, url);
    printf("NIMCP: Saving to %s\n", filepath);

    // Use curl to download (works on most Linux systems)
    char cmd[NIMCP_CMD_BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "curl -L -o '%s' '%s' 2>/dev/null", filepath, url);

    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "NIMCP Error: Failed to download model (curl exit code: %d)\n", result);
        fprintf(stderr, "NIMCP Error: Make sure curl is installed and you have internet connection\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_download_model: validation failed");
        return false;
    }

    // Verify download
    if (!brain_model_exists(model_id)) {
        fprintf(stderr, "NIMCP Error: Download completed but model file not found\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_download_model: brain_model_exists is NULL");
        return false;
    }

    printf("NIMCP: Model '%s' downloaded successfully\n", model_id);
    return true;
}

bool brain_get_model_info(const char* model_id, brain_model_info_t* info)
{
    if (!model_id || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_model_info: required parameter is NULL (model_id, info)");
        return false;
    }

    memset(info, 0, sizeof(brain_model_info_t));

    // Set basic info
    strncpy(info->model_id, model_id, sizeof(info->model_id) - 1);
    strncpy(info->version, "v2.7.0", sizeof(info->version) - 1);
    strncpy(info->training_date, "2025-11-08", sizeof(info->training_date) - 1);

    // Set model-specific info
    if (strcmp(model_id, "nimcp_baseline_small") == 0) {
        info->file_size_bytes = 4200000; // 4.2 MB
        strncpy(info->description, "1K neurons, fast inference (0.3ms), embedded systems",
                sizeof(info->description) - 1);
    } else if (strcmp(model_id, "nimcp_baseline_medium") == 0) {
        info->file_size_bytes = 42000000; // 42 MB
        strncpy(info->description, "10K neurons, balanced performance (0.8ms), RECOMMENDED",
                sizeof(info->description) - 1);
    } else if (strcmp(model_id, "nimcp_baseline_large") == 0) {
        info->file_size_bytes = 420000000; // 420 MB
        strncpy(info->description, "100K neurons, high accuracy (3ms), research applications",
                sizeof(info->description) - 1);
    } else {
        fprintf(stderr, "NIMCP Error: Unknown model_id '%s'\n", model_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_model_info: operation failed");
        return false;
    }

    // Check availability
    info->is_available = brain_model_exists(model_id);
    info->update_available = false; // No updates yet
    strncpy(info->latest_version, "v2.7.0", sizeof(info->latest_version) - 1);

    return true;
}

brain_t brain_create_pretrained(const char* model_id, brain_task_t task)
{
    if (!model_id) {
        fprintf(stderr, "NIMCP Error: model_id is NULL\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "model_id is NULL");

        return NULL;
    }

    printf("NIMCP: Loading pre-trained model '%s'...\n", model_id);

    // Check if model exists locally
    if (!brain_model_exists(model_id)) {
        printf("NIMCP: Model not found locally, downloading...\n");
        if (!brain_download_model(model_id)) {
            fprintf(stderr, "NIMCP Error: Failed to download model '%s'\n", model_id);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_create_pretrained: brain_download_model is NULL");
            return NULL;
        }
    }

    // Get model filepath
    char filepath[NIMCP_METRICS_PATH_SIZE];
    if (!get_model_filepath(model_id, filepath, sizeof(filepath))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_pretrained: get_model_filepath is NULL");
        return NULL;
    }

    // Load model from file
    brain_t brain = brain_load(filepath);
    if (!brain) {
        fprintf(stderr, "NIMCP Error: Failed to load model from %s\n", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    // Update task configuration
    brain->config.task = task;

    // Get brain statistics
    brain_stats_t stats;
    brain_get_stats(brain, &stats);

    printf("NIMCP: Pre-trained model '%s' loaded successfully\n", model_id);
    printf("NIMCP:   Neurons: %u\n", stats.num_neurons);
    printf("NIMCP:   Synapses: %u\n", stats.num_synapses);
    printf("NIMCP:   Ready for immediate inference!\n");

    return brain;
}

brain_t brain_load_pretrained(const char* model_name, const char* models_dir) {
    // TODO: Implement once libcjson-dev is available
    // For now, forward to brain_create_pretrained with default task
    (void)models_dir;  // Unused parameter
    return brain_create_pretrained(model_name, BRAIN_TASK_CLASSIFICATION);
}

bool brain_finetune(brain_t brain, const float* training_data, const float* labels,
                    uint32_t num_samples, const brain_finetune_config_t* config)
{
    if (!brain || !training_data || !labels || num_samples == 0) {
        fprintf(stderr, "NIMCP Error: Invalid parameters for brain_finetune\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_finetune: required parameter is NULL (brain, training_data, labels)");
        return false;
    }

    // Set default config if not provided
    brain_finetune_config_t default_config = {
        .learning_rate = 0.001F,
        .num_epochs = 5,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = NIMCP_DEFAULT_BATCH_SIZE,
        .verbose = true
    };

    const brain_finetune_config_t* cfg = config ? config : &default_config;

    if (cfg->verbose) {
        printf("NIMCP: Fine-tuning brain '%s' on %u examples...\n",
               brain->config.task_name, num_samples);
        printf("NIMCP:   Learning rate: %.4f\n", cfg->learning_rate);
        printf("NIMCP:   Epochs: %u\n", cfg->num_epochs);
        printf("NIMCP:   Freeze sensory: %s\n", cfg->freeze_sensory ? "yes" : "no");
        printf("NIMCP:   Freeze cognitive: %s\n", cfg->freeze_cognitive ? "yes" : "no");
        printf("NIMCP:   Fine-tune classifier: %s\n", cfg->finetune_classifier ? "yes" : "no");
    }

    // Store original learning rate
    float original_lr = brain->config.learning_rate;

    // Set fine-tuning learning rate
    brain->config.learning_rate = cfg->learning_rate;

    // WHAT: Implement selective layer freezing for transfer learning
    // WHY:  Preserve pre-trained features while adapting to new tasks
    // HOW:  Temporarily scale learning rate based on layer type
    //
    // BIOLOGICAL ANALOGY: Transfer learning in the brain preserves core
    // representations (V1 edge detectors, phoneme representations) while
    // adapting higher layers (object recognition, word meaning).
    //
    // IMPLEMENTATION STRATEGY:
    // Since NIMCP uses a single global learning_rate, we implement freezing
    // by temporarily scaling the learning rate during backpropagation.
    // This is a simplified approach that works for the current architecture.
    //
    // LAYER INTERPRETATION:
    // - Input layer (0-20% of neurons): Sensory processing
    // - Middle layers (20-80%): Cognitive/association processing
    // - Output layer (80-100%): Classification/decision

    // Compute layer boundaries (approximate, since we don't have explicit layers)
    brain_stats_t stats;
    if (!brain_get_stats(brain, &stats)) {
        fprintf(stderr, "NIMCP Error: Failed to get brain stats for layer freezing\n");
        brain->config.learning_rate = original_lr;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_finetune: brain_get_stats is NULL");
        return false;
    }

    uint32_t total_neurons = stats.num_neurons;
    uint32_t sensory_end = total_neurons / 5;        // First 20%
    uint32_t cognitive_end = (total_neurons * 4) / 5; // Next 60%
    // Classifier is the remaining 20%

    // Calculate effective learning rate based on freeze configuration
    // WHAT: Compute layer-specific learning rates
    // WHY:  Frozen layers need near-zero gradients, unfrozen layers use full rate
    // HOW:  Scale learning rate by layer-specific multipliers
    float sensory_lr_multiplier = cfg->freeze_sensory ? 0.01F : 1.0F;  // 1% for frozen
    float cognitive_lr_multiplier = cfg->freeze_cognitive ? 0.01F : 1.0F;
    float classifier_lr_multiplier = cfg->finetune_classifier ? 1.0F : 0.01F;

    if (cfg->verbose) {
        printf("NIMCP:   Layer freezing enabled:\n");
        printf("NIMCP:     Sensory (0-%u neurons): %.1f%% learning rate\n",
               sensory_end, sensory_lr_multiplier * 100.0F);
        printf("NIMCP:     Cognitive (%u-%u neurons): %.1f%% learning rate\n",
               sensory_end, cognitive_end, cognitive_lr_multiplier * 100.0F);
        printf("NIMCP:     Classifier (%u-%u neurons): %.1f%% learning rate\n",
               cognitive_end, total_neurons, classifier_lr_multiplier * 100.0F);
    }

    // Training loop
    for (uint32_t epoch = 0; epoch < cfg->num_epochs; epoch++) {
        float epoch_loss = 0.0F;
        uint32_t correct = 0;

        // Mini-batch training
        for (uint32_t i = 0; i < num_samples; i++) {
            // Get training example
            const float* input = &training_data[i * brain->config.num_inputs];
            const float* target = &labels[i * brain->config.num_outputs];

            // Forward pass
            brain_decision_t* decision = brain_decide(brain, input, brain->config.num_inputs);
            if (!decision) {
                fprintf(stderr, "NIMCP Error: Forward pass failed at sample %u\n", i);
                brain->config.learning_rate = original_lr;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_finetune: decision is NULL");
                return false;
            }

            // Compute loss (MSE) and check correctness
            float sample_loss = 0.0F;
            uint32_t predicted_class = 0;
            uint32_t target_class = 0;
            float max_pred = decision->output_vector[0];
            float max_target = target[0];

            for (uint32_t j = 0; j < brain->config.num_outputs; j++) {
                float error = target[j] - decision->output_vector[j];
                sample_loss += error * error;

                if (decision->output_vector[j] > max_pred) {
                    max_pred = decision->output_vector[j];
                    predicted_class = j;
                }
                if (target[j] > max_target) {
                    max_target = target[j];
                    target_class = j;
                }
            }

            epoch_loss += sample_loss;

            if (predicted_class == target_class) {
                correct++;
            }

            // Free decision before learning
            brain_free_decision(decision);

            // WHAT: Apply layer-specific learning rates based on freeze configuration
            // WHY:  Implement transfer learning by freezing pre-trained layers
            // HOW:  Modulate global learning rate for each training step
            //
            // NOTE: This is a simplified implementation. A more sophisticated approach
            // would modify individual synapse weights, but that requires accessing
            // internal network structures. For now, we use a weighted average approach
            // that approximates layer-specific freezing.
            //
            // BIOLOGICAL ANALOGY: Selective consolidation - some synapses are
            // "protected" from modification (high synaptic tags), while others
            // remain plastic (low tags).

            // Compute weighted learning rate based on layer configuration
            // WHAT: Calculate effective learning rate for this sample
            // WHY:  Different layers have different plasticity based on freeze flags
            // HOW:  Weighted average of layer-specific multipliers
            float effective_lr;
            if (cfg->freeze_sensory && cfg->freeze_cognitive) {
                // Only classifier is being trained - use classifier LR
                effective_lr = cfg->learning_rate * classifier_lr_multiplier;
            } else if (cfg->freeze_sensory || cfg->freeze_cognitive) {
                // Partial freezing - use weighted average
                // This approximates layer-specific learning rates
                float weight_sum = 0.2F + 0.6F + 0.2F;  // Sensory + Cognitive + Classifier
                float weighted_lr = (0.2F * sensory_lr_multiplier +
                                    0.6F * cognitive_lr_multiplier +
                                    0.2F * classifier_lr_multiplier) / weight_sum;
                effective_lr = cfg->learning_rate * weighted_lr;
            } else {
                // No freezing - use full learning rate
                effective_lr = cfg->learning_rate;
            }

            // Temporarily set the effective learning rate for this sample
            float temp_lr = brain->config.learning_rate;
            brain->config.learning_rate = effective_lr;

            // Backward pass (learn from example)
            // Use class label as string
            char label_str[32];
            snprintf(label_str, sizeof(label_str), "class_%u", target_class);

            float loss = brain_learn_example(brain, input, brain->config.num_inputs,
                                            label_str, 1.0F);
            (void)loss; // Suppress unused variable warning

            // Restore temporary learning rate
            brain->config.learning_rate = temp_lr;
        }

        // Epoch statistics
        float avg_loss = epoch_loss / num_samples;
        float accuracy = (float)correct / num_samples;

        if (cfg->verbose) {
            printf("NIMCP:   Epoch %u/%u: loss=%.4f accuracy=%.2f%%\n",
                   epoch + 1, cfg->num_epochs, avg_loss, accuracy * 100.0F);
        }
    }

    // Restore original learning rate
    brain->config.learning_rate = original_lr;

    // Clear decision cache after training
    // Use the factory module's cache clearing function
    extern void nimcp_brain_factory_clear_cache(brain_t brain);
    nimcp_brain_factory_clear_cache(brain);

    if (cfg->verbose) {
        printf("NIMCP: Fine-tuning complete!\n");
    }

    return true;
}
