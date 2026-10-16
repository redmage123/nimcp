//=============================================================================
// nimcp_pretrained.h - Pre-trained Model Loading API
//=============================================================================
/**
 * @file nimcp_pretrained.h
 * @brief Public API for loading and using pre-trained NIMCP models
 *
 * WHAT: Interface for pre-trained model management
 * WHY:  Enable instant deployment without lengthy training
 * HOW:  Model repository + metadata + deserialization
 *
 * @author NIMCP Development Team
 * @date 2025-11-09
 * @version 1.0.0
 */

#ifndef NIMCP_PRETRAINED_H
#define NIMCP_PRETRAINED_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Pre-trained Model Loading
//=============================================================================

/**
 * @brief Load pre-trained NIMCP model from repository
 *
 * WHAT: Loads a pre-trained brain with trained weights and topology
 * WHY:  Instant deployment without 48-hour training
 * HOW:  Metadata validation + deserialization + integrity check
 *
 * MODEL NAMING:
 * Format: "nimcp_<type>_<size>_<version>"
 * Examples:
 *   - "nimcp_foundation_small_v1.0"   - Small general-purpose model
 *   - "nimcp_foundation_medium_v1.0"  - Medium general-purpose (recommended)
 *   - "nimcp_foundation_large_v1.0"   - Large high-accuracy model
 *   - "nimcp_ethics_medium_v1.0"      - Ethics-specialized model
 *   - "nimcp_multimodal_large_v1.0"   - Multi-modal (vision+audio+speech)
 *
 * REPOSITORY SEARCH ORDER:
 * 1. NIMCP_MODELS_DIR environment variable
 * 2. Source repository: /path/to/nimcp/models/pretrained/
 * 3. User home: ~/.nimcp/models/pretrained/
 * 4. System-wide: /usr/local/share/nimcp/models/pretrained/
 *
 * ERROR HANDLING:
 * Returns NULL if:
 * - Model not found in any repository
 * - Metadata invalid or missing
 * - Checksum validation failed
 * - Deserialization error
 *
 * @param model_name Model identifier (e.g., "nimcp_foundation_medium_v1.0")
 * @param models_dir Optional custom models directory (NULL = use defaults)
 * @return Brain handle or NULL on error
 *
 * @example
 * ```c
 * // Load pre-trained model
 * brain_t brain = brain_load_pretrained("nimcp_foundation_medium_v1.0", NULL);
 * if (!brain) {
 *     fprintf(stderr, "Failed to load model\n");
 *     return -1;
 * }
 *
 * // Use immediately for inference
 * brain_decision_t* decision = brain_decide(brain, features, num_features);
 * printf("Prediction: %s (%.2f confidence)\n",
 *        decision->label, decision->confidence);
 *
 * // Optional: Fine-tune on your data
 * brain_learn_example(brain, task_features, num_features, label, 1.0f);
 *
 * // Save fine-tuned version
 * brain_save(brain, "my_finetuned_model.nimcp");
 *
 * brain_destroy(brain);
 * ```
 */
brain_t brain_load_pretrained(const char* model_name, const char* models_dir);

//=============================================================================
// Model Information and Discovery
//=============================================================================

/**
 * @brief Get detailed information about a pre-trained model
 *
 * @param model_id Model identifier
 * @param info Output: model information structure
 * @return true on success, false if model not found
 *
 * @example
 * ```c
 * brain_model_info_t info;
 * if (brain_get_model_info("nimcp_foundation_medium_v1.0", &info)) {
 *     printf("Model: %s\n", info.model_id);
 *     printf("Version: %s\n", info.version);
 *     printf("Size: %zu MB\n", info.file_size_bytes / (1024 * 1024));
 *     printf("Available: %s\n", info.is_available ? "Yes" : "No");
 * }
 * ```
 */
bool brain_get_model_info(const char* model_id, brain_model_info_t* info);

/**
 * @brief Check if model exists in local cache
 *
 * @param model_id Model identifier
 * @return true if model is available locally
 */
bool brain_model_exists(const char* model_id);

/**
 * @brief Download pre-trained model from remote repository
 *
 * WHAT: Downloads model binary and metadata from NIMCP model repository
 * WHY:  Enable first-time model acquisition
 * HOW:  HTTPS download + SHA256 verification + local caching
 *
 * @param model_id Model identifier
 * @return true on success
 *
 * @note Currently not implemented. Models should be:
 *       1. Trained using NIMCP training scripts
 *       2. Saved using brain_save()
 *       3. Placed in models/pretrained/<size>/<version>/
 */
bool brain_download_model(const char* model_id);

//=============================================================================
// Fine-tuning API
//=============================================================================

/**
 * @brief Fine-tune pre-trained model on domain-specific data
 *
 * WHAT: Adapts pre-trained baseline to specific task/domain
 * WHY:  Bridge gap between general baseline and domain requirements
 * HOW:  Selective layer freezing + lower learning rate + few-shot learning
 *
 * STRATEGIES:
 *
 * 1. **Quick Adaptation** (10-100 examples, ~10 minutes)
 *    - Freeze all layers except classifier
 *    - Use for task-specific output labels
 *    ```c
 *    brain_finetune_config_t config = {
 *        .freeze_sensory = true,
 *        .freeze_cognitive = true,
 *        .finetune_classifier = true,
 *        .num_epochs = 5
 *    };
 *    ```
 *
 * 2. **Domain Adaptation** (100-1000 examples, ~1 hour)
 *    - Unfreeze sensory cortices
 *    - Keep cognitive modules frozen
 *    - Use for domain-specific feature extraction
 *    ```c
 *    brain_finetune_config_t config = {
 *        .freeze_sensory = false,
 *        .freeze_cognitive = true,
 *        .learning_rate = 0.0005
 *    };
 *    ```
 *
 * 3. **Full Fine-tuning** (1000+ examples, several hours)
 *    - Unfreeze all layers
 *    - Very low learning rate to preserve pre-trained knowledge
 *    ```c
 *    brain_finetune_config_t config = {
 *        .freeze_sensory = false,
 *        .freeze_cognitive = false,
 *        .learning_rate = 0.0001
 *    };
 *    ```
 *
 * @param brain Pre-trained brain to fine-tune
 * @param training_data Array of input examples [num_samples × input_dim]
 * @param labels Array of target labels/outputs [num_samples × output_dim]
 * @param num_samples Number of training examples
 * @param config Fine-tuning configuration (NULL = use defaults)
 * @return true on success
 *
 * @example
 * ```c
 * brain_t brain = brain_load_pretrained("nimcp_foundation_medium_v1.0", NULL);
 *
 * // Fine-tune on 100 domain-specific examples
 * brain_finetune_config_t config = {
 *     .learning_rate = 0.001,
 *     .num_epochs = 10,
 *     .freeze_sensory = true,
 *     .freeze_cognitive = true,
 *     .finetune_classifier = true,
 *     .batch_size = 32,
 *     .verbose = true
 * };
 *
 * if (brain_finetune(brain, train_data, train_labels, 100, &config)) {
 *     printf("Fine-tuning successful!\n");
 *     brain_save(brain, "finetuned_model.nimcp");
 * }
 * ```
 */
bool brain_finetune(brain_t brain, const float* training_data, const float* labels,
                   uint32_t num_samples, const brain_finetune_config_t* config);

/**
 * @brief Create brain from pre-trained model (convenience wrapper)
 *
 * Wrapper for brain_load_pretrained() that matches the brain_create_pretrained()
 * signature from the main brain API.
 *
 * @param model_id Model identifier
 * @param task Task template (reserved for future use)
 * @return Brain handle or NULL on error
 */
brain_t brain_create_pretrained(const char* model_id, brain_task_t task);

//=============================================================================
// Memory and Performance Monitoring
//=============================================================================

/**
 * @brief Get brain memory footprint
 *
 * WHAT: Calculate total memory usage of brain
 * WHY:  Monitor resource usage, select appropriate model size
 * HOW:  Sum of neuron states + synaptic weights + metadata
 *
 * @param brain Brain handle
 * @return Memory usage in bytes
 *
 * @example
 * ```c
 * brain_t brain = brain_load_pretrained("nimcp_foundation_large_v1.0", NULL);
 * size_t memory_mb = brain_get_memory_usage(brain) / (1024 * 1024);
 * printf("Model memory usage: %zu MB\n", memory_mb);
 *
 * if (memory_mb > 100) {
 *     printf("Warning: Consider using medium model for lower memory usage\n");
 * }
 * ```
 */
size_t brain_get_memory_usage(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PRETRAINED_H
