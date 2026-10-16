//=============================================================================
// nimcp_multimodal_integration.h - Multi-Modal Sensory Integration
//=============================================================================
/**
 * @file nimcp_multimodal_integration.h
 * @brief Integrate visual, audio, and direct inputs into unified representation
 *
 * WHAT: Multi-modal integration layer for combining sensory streams
 * WHY:  Brain must integrate vision + audio + other inputs before processing
 * HOW:  Feature concatenation + attention + dimensionality reduction
 *
 * BIOLOGICAL MOTIVATION:
 * - Superior Temporal Sulcus: Multi-modal integration (audio-visual)
 * - Posterior Parietal Cortex: Sensory-motor integration
 * - Prefrontal Cortex: Top-down attention modulation
 *
 * ARCHITECTURE:
 * 1. Feature Extraction: Visual/audio cortices extract features
 * 2. Concatenation: [visual|audio|direct] → large feature vector
 * 3. Attention: Weight modalities by salience/relevance
 * 4. Integration: Fuse into compact unified representation
 * 5. Output: Feed into main neural network input layer
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 8
 */

#ifndef NIMCP_MULTIMODAL_INTEGRATION_H
#define NIMCP_MULTIMODAL_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Integration method
 */
typedef enum {
    INTEGRATION_CONCATENATE,  /**< Simple concatenation */
    INTEGRATION_ATTENTION,    /**< Attention-weighted fusion */
    INTEGRATION_LEARNED       /**< Learned integration weights */
} integration_method_t;

/**
 * @brief Multi-modal integration configuration
 */
typedef struct {
    uint32_t visual_dim;      /**< Visual feature dimension (0 = disabled) */
    uint32_t audio_dim;       /**< Audio feature dimension (0 = disabled) */
    uint32_t speech_dim;      /**< Speech feature dimension (0 = disabled) - Phase 8.8 */
    uint32_t direct_dim;      /**< Direct input dimension (0 = disabled) */
    uint32_t output_dim;      /**< Output dimension (network input size) */
    integration_method_t method; /**< Integration method */
    float visual_weight;      /**< Visual modality weight (0-1) */
    float audio_weight;       /**< Audio modality weight (0-1) */
    float speech_weight;      /**< Speech modality weight (0-1) - Phase 8.8 */
    float direct_weight;      /**< Direct input weight (0-1) */
} multimodal_config_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Multi-modal input bundle
 */
typedef struct {
    float* visual_features;   /**< Visual features (can be NULL) */
    uint32_t visual_dim;      /**< Visual dimension */
    float* audio_features;    /**< Audio features (can be NULL) */
    uint32_t audio_dim;       /**< Audio dimension */
    float* speech_features;   /**< Speech features (can be NULL) - Phase 8.8 */
    uint32_t speech_dim;      /**< Speech dimension */
    float* direct_features;   /**< Direct features (can be NULL) */
    uint32_t direct_dim;      /**< Direct dimension */
    uint64_t timestamp;       /**< Timestamp for temporal alignment */
} multimodal_input_t;

/**
 * @brief Integrated output
 */
typedef struct {
    float* integrated_features; /**< Integrated representation */
    uint32_t integrated_dim;    /**< Output dimension */
    float visual_attention;     /**< Learned visual attention weight */
    float audio_attention;      /**< Learned audio attention weight */
    float speech_attention;     /**< Learned speech attention weight - Phase 8.8 */
    float direct_attention;     /**< Learned direct attention weight */
} multimodal_output_t;

/**
 * @brief Opaque integration handle
 */
typedef struct multimodal_integration_struct* multimodal_integration_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * WHAT: Create multi-modal integration system
 * WHY:  Initialize integration layer for unified processing
 * HOW:  Allocate buffers and integration weights
 *
 * @param config Integration configuration
 * @return Integration handle or NULL on failure
 *
 * COMPLEXITY: O(D_out) where D_out = output dimension
 */
multimodal_integration_t multimodal_integration_create(const multimodal_config_t* config);

/**
 * WHAT: Destroy integration system
 * WHY:  Free allocated resources
 *
 * @param integration Integration handle
 */
void multimodal_integration_destroy(multimodal_integration_t integration);

//=============================================================================
// Integration
//=============================================================================

/**
 * WHAT: Integrate multi-modal inputs into unified representation
 * WHY:  Create single feature vector for neural network
 * HOW:  Apply configured integration method
 *
 * @param integration Integration handle
 * @param input Multi-modal inputs
 * @param output Output buffer (pre-allocated to output_dim)
 * @return true on success, false on failure
 *
 * COMPLEXITY:
 * - CONCATENATE: O(D_v + D_a + D_d) - simple copy
 * - ATTENTION: O(D_v + D_a + D_d + D_out) - weighted fusion
 * - LEARNED: O(D_v·D_out + D_a·D_out + D_d·D_out) - matrix multiplication
 *
 * USAGE:
 * ```c
 * multimodal_input_t input = {
 *     .visual_features = visual_features,
 *     .visual_dim = 128,
 *     .audio_features = audio_features,
 *     .audio_dim = 64,
 *     .direct_features = NULL,
 *     .direct_dim = 0
 * };
 * float output[256];
 * multimodal_integrate(integration, &input, output);
 * // output now contains unified representation
 * ```
 */
bool multimodal_integrate(
    multimodal_integration_t integration,
    const multimodal_input_t* input,
    float* output
);

/**
 * WHAT: Get attention weights for last integration
 * WHY:  Understand which modalities were most important
 * HOW:  Return learned/computed attention values
 *
 * @param integration Integration handle
 * @param visual_attn Output: visual attention weight
 * @param audio_attn Output: audio attention weight
 * @param speech_attn Output: speech attention weight (Phase 8.8)
 * @param direct_attn Output: direct attention weight
 * @return true on success
 */
bool multimodal_get_attention(
    const multimodal_integration_t integration,
    float* visual_attn,
    float* audio_attn,
    float* speech_attn,
    float* direct_attn
);

/**
 * WHAT: Update integration weights based on feedback
 * WHY:  Learn optimal integration strategy
 * HOW:  Gradient descent on attention weights
 *
 * @param integration Integration handle
 * @param reward Reward signal (-1 to 1)
 * @param learning_rate Learning rate for weight update
 * @return true on success
 */
bool multimodal_update_weights(
    multimodal_integration_t integration,
    float reward,
    float learning_rate
);

//=============================================================================
// Helpers
//=============================================================================

/**
 * WHAT: Get default configuration
 * WHY:  Sensible defaults for common use cases
 *
 * @param visual_dim Visual dimension
 * @param audio_dim Audio dimension
 * @param speech_dim Speech dimension (Phase 8.8)
 * @param direct_dim Direct dimension
 * @return Default configuration
 */
multimodal_config_t multimodal_default_config(
    uint32_t visual_dim,
    uint32_t audio_dim,
    uint32_t speech_dim,
    uint32_t direct_dim
);

/**
 * WHAT: Validate input against configuration
 * WHY:  Catch dimension mismatches
 *
 * @param integration Integration handle
 * @param input Input to validate
 * @return true if valid
 */
bool multimodal_validate_input(
    const multimodal_integration_t integration,
    const multimodal_input_t* input
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MULTIMODAL_INTEGRATION_H
