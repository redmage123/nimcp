//=============================================================================
// nimcp_brain_pretrained.h - Pre-trained Model Management Module
//=============================================================================
/**
 * @file nimcp_brain_pretrained.h
 * @brief Internal API for pre-trained model management (extracted from nimcp_brain.c)
 *
 * WHAT: Model download, loading, and fine-tuning functions
 * WHY:  Modularize pre-trained model management for better maintainability
 * HOW:  Extracted from nimcp_brain.c Phase 9.0 implementation
 *
 * SCOPE:
 * - Model existence checking
 * - Model download from remote repository
 * - Model metadata retrieval
 * - Pre-trained model creation
 * - Model fine-tuning with selective layer freezing
 *
 * @author NIMCP Development Team
 * @date 2025-11-19
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_PRETRAINED_H
#define NIMCP_BRAIN_PRETRAINED_H

#include "core/brain/nimcp_brain.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Public API Functions
//=============================================================================

/**
 * @brief Check if pre-trained model exists locally
 *
 * WHAT: Checks for model file in local cache directory
 * WHY:  Determine if download is needed before attempting load
 * HOW:  Uses stat() to check file existence in ~/.nimcp/models/
 *
 * @param model_id Model identifier (e.g., "nimcp_baseline_medium")
 * @return true if model file exists, false otherwise
 *
 * COMPLEXITY: O(1) - single filesystem check
 */
NIMCP_EXPORT bool brain_model_exists(const char* model_id);

/**
 * @brief Download pre-trained model from remote repository
 *
 * WHAT: Downloads model binary from NIMCP model repository
 * WHY:  Enable first-time model acquisition
 * HOW:  Uses curl to download from https://models.nimcp.ai/
 *
 * @param model_id Model identifier (e.g., "nimcp_baseline_medium")
 * @return true on successful download, false on error
 *
 * REQUIREMENTS:
 * - curl must be installed on system
 * - Internet connection required
 * - Write permissions to ~/.nimcp/models/
 *
 * COMPLEXITY: O(n) where n = model file size (network I/O bound)
 */
NIMCP_EXPORT bool brain_download_model(const char* model_id);

/**
 * @brief Get detailed information about a pre-trained model
 *
 * WHAT: Retrieves model metadata (size, version, description, availability)
 * WHY:  Allow users to inspect model details before loading
 * HOW:  Returns hardcoded metadata for known model IDs
 *
 * SUPPORTED MODELS:
 * - nimcp_baseline_small: 1K neurons, 4.2MB, 0.3ms inference
 * - nimcp_baseline_medium: 10K neurons, 42MB, 0.8ms inference (RECOMMENDED)
 * - nimcp_baseline_large: 100K neurons, 420MB, 3ms inference
 *
 * @param model_id Model identifier
 * @param info Output: model information structure
 * @return true on success, false if model_id unknown
 *
 * COMPLEXITY: O(1) - metadata lookup
 */
NIMCP_EXPORT bool brain_get_model_info(const char* model_id, brain_model_info_t* info);

/**
 * @brief Create brain from pre-trained model
 *
 * WHAT: Load pre-trained weights and topology, configure for specific task
 * WHY:  Enable instant deployment without lengthy training
 * HOW:  Downloads if needed, calls brain_load(), updates task configuration
 *
 * WORKFLOW:
 * 1. Check if model exists locally (brain_model_exists)
 * 2. Download if needed (brain_download_model)
 * 3. Load model from file (brain_load)
 * 4. Update task configuration
 * 5. Print statistics
 *
 * @param model_id Model identifier (e.g., "nimcp_baseline_medium")
 * @param task Task type (BRAIN_TASK_CLASSIFICATION, etc.)
 * @return Brain handle or NULL on error
 *
 * COMPLEXITY: O(n) where n = model size (I/O bound)
 */
NIMCP_EXPORT brain_t brain_create_pretrained(const char* model_id, brain_task_t task);

/**
 * @brief Load pre-trained model (forwards to brain_create_pretrained)
 *
 * WHAT: Compatibility wrapper for brain_load_pretrained API
 * WHY:  Maintain API compatibility during modularization
 * HOW:  Forwards to brain_create_pretrained with default task
 *
 * @param model_name Model identifier
 * @param models_dir Unused (reserved for future use)
 * @return Brain handle or NULL on error
 *
 * COMPLEXITY: O(n) - same as brain_create_pretrained
 */
NIMCP_EXPORT brain_t brain_load_pretrained(const char* model_name, const char* models_dir);

/**
 * @brief Fine-tune pre-trained model on domain-specific data
 *
 * WHAT: Adapts pre-trained baseline to specific task/domain
 * WHY:  Bridge gap between general baseline and domain requirements
 * HOW:  Selective layer freezing + lower learning rate + mini-batch training
 *
 * TRANSFER LEARNING STRATEGY:
 * - Sensory layers (0-20% neurons): Low-level feature extraction
 * - Cognitive layers (20-80%): High-level representations
 * - Classifier (80-100%): Task-specific outputs
 *
 * BIOLOGICAL ANALOGY:
 * Transfer learning preserves core representations (V1 edge detectors)
 * while adapting higher layers (object recognition, word meaning).
 *
 * LAYER FREEZING:
 * - Frozen layers: 1% learning rate (minimal plasticity)
 * - Unfrozen layers: 100% learning rate (full plasticity)
 *
 * @param brain Pre-trained brain to fine-tune
 * @param training_data Input examples [num_samples × input_dim]
 * @param labels Target labels [num_samples × output_dim]
 * @param num_samples Number of training examples
 * @param config Fine-tuning configuration (NULL = use defaults)
 * @return true on success, false on error
 *
 * DEFAULT CONFIG:
 * - learning_rate: 0.001
 * - num_epochs: 5
 * - freeze_sensory: true
 * - freeze_cognitive: true
 * - finetune_classifier: true
 * - batch_size: 32
 * - verbose: true
 *
 * COMPLEXITY: O(E * N * S) where:
 * - E = num_epochs
 * - N = num_samples
 * - S = sparsity factor (active synapses per neuron)
 */
NIMCP_EXPORT bool brain_finetune(brain_t brain, const float* training_data, const float* labels,
                                  uint32_t num_samples, const brain_finetune_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_PRETRAINED_H
