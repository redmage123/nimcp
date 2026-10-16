//=============================================================================
// sensory_extractor.h - Sensory Feature Extraction Module
//=============================================================================
/**
 * @file sensory_extractor.h
 * @brief Single Responsibility: Extract features from raw sensory inputs
 *
 * WHAT: Converts high-dimensional sensory data to compact neural representations
 * WHY:  Separates sensory processing from integration/decision logic (SRP)
 * HOW:  Uses specialized cortices (V1 visual, A1 audio, STG speech)
 *
 * RESPONSIBILITIES:
 * - Process visual data through visual cortex (CNN-based V1)
 * - Process audio data through auditory cortex (FFT-based A1)
 * - Process speech from audio through speech cortex (STG/Wernicke)
 * - Return compact feature vectors for each modality
 *
 * NON-RESPONSIBILITIES (delegated to other modules):
 * - Multimodal integration
 * - Neural network inference
 * - Cognitive processing
 * - Output formatting
 */

#ifndef NIMCP_SENSORY_EXTRACTOR_H
#define NIMCP_SENSORY_EXTRACTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;

//=============================================================================
// Sensory Input Structure
//=============================================================================

/**
 * @brief Raw sensory inputs (multiple modalities)
 */
typedef struct {
    // Visual modality
    const uint8_t* visual_data;
    uint32_t visual_width;
    uint32_t visual_height;
    uint32_t visual_channels;

    // Audio modality
    const float* audio_data;
    uint32_t audio_samples;
    uint32_t audio_channels;

    // Direct neural features (bypass sensory processing)
    const float* direct_data;
    uint32_t direct_dim;
} sensory_input_t;

//=============================================================================
// Extracted Features Structure
//=============================================================================

/**
 * @brief Extracted sensory features (compact representations)
 */
typedef struct {
    // Visual features (from V1 cortex)
    float* visual_features;
    uint32_t visual_dim;
    bool visual_valid;

    // Audio features (from A1 cortex)
    float* audio_features;
    uint32_t audio_dim;
    bool audio_valid;

    // Speech features (from STG/Wernicke cortex, hierarchical from audio)
    float* speech_features;
    uint32_t speech_dim;
    bool speech_valid;

    // Direct features (pass-through)
    const float* direct_features;
    uint32_t direct_dim;
    bool direct_valid;
} sensory_features_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Extract features from raw sensory inputs
 *
 * WHAT: Single responsibility - sensory feature extraction only
 * WHY:  Isolated, testable, reusable sensory processing
 * HOW:  Routes inputs to appropriate cortices, returns feature vectors
 *
 * @param brain Brain with initialized cortices
 * @param input Raw sensory inputs
 * @param features Output extracted features (allocated by caller)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = input size (dominated by cortex processing)
 * THREAD-SAFETY: Read-only access to brain cortices (thread-safe if brain is read-only)
 */
bool sensory_extract_features(
    const brain_t brain,
    const sensory_input_t* input,
    sensory_features_t* features);

/**
 * @brief Validate sensory input
 *
 * @param input Sensory input to validate
 * @return true if at least one modality is present
 */
bool sensory_input_validate(const sensory_input_t* input);

/**
 * @brief Initialize sensory features structure
 *
 * @param features Features structure to initialize
 */
void sensory_features_init(sensory_features_t* features);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SENSORY_EXTRACTOR_H
