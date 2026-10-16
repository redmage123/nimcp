//=============================================================================
// multimodal_integrator.h - Multimodal Feature Integration Module
//=============================================================================
/**
 * @file multimodal_integrator.h
 * @brief Single Responsibility: Fuse multi-modal features into unified representation
 *
 * WHAT: Combines visual, audio, speech, and direct features via attention
 * WHY:  Separates integration logic from sensory extraction and inference (SRP)
 * HOW:  4-way attention mechanism (like Transformer cross-attention)
 *
 * RESPONSIBILITIES:
 * - Integrate multi-modal features using attention weights
 * - Compute attention weights for each modality
 * - Produce unified feature vector for neural network
 *
 * NON-RESPONSIBILITIES (delegated to other modules):
 * - Sensory feature extraction
 * - Neural network inference
 * - Cognitive processing
 */

#ifndef NIMCP_MULTIMODAL_INTEGRATOR_H
#define NIMCP_MULTIMODAL_INTEGRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/processing/sensory_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;

//=============================================================================
// Multimodal Integration Output
//=============================================================================

/**
 * @brief Integrated multi-modal features
 */
typedef struct {
    float* integrated_features;   /**< Unified feature vector */
    uint32_t integrated_dim;      /**< Dimension of integrated vector */

    // Attention weights (transparency/interpretability)
    float visual_attention;       /**< Visual modality weight (0-1) */
    float audio_attention;        /**< Audio modality weight (0-1) */
    float speech_attention;       /**< Speech modality weight (0-1) */
    float direct_attention;       /**< Direct modality weight (0-1) */
} integrated_features_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Integrate multi-modal features using attention
 *
 * WHAT: Single responsibility - feature integration only
 * WHY:  Isolated, testable attention-based fusion
 * HOW:  4-way attention mechanism (visual, audio, speech, direct)
 *
 * @param brain Brain with initialized multimodal integrator
 * @param features Extracted sensory features from all modalities
 * @param output Integrated features with attention weights (allocated by caller)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n*m) where n=num_modalities, m=feature_dim
 * THREAD-SAFETY: Read-only access to brain (thread-safe if brain is read-only)
 */
bool multimodal_integrate_features(
    const brain_t brain,
    const sensory_features_t* features,
    integrated_features_t* output);

/**
 * @brief Initialize integrated features structure
 *
 * @param output Integrated features structure to initialize
 */
void integrated_features_init(integrated_features_t* output);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MULTIMODAL_INTEGRATOR_H
