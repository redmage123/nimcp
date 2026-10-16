//=============================================================================
// nimcp_multimodal_nlp_bridge.h - Multimodal-to-NLP Integration Bridge
//=============================================================================
/**
 * @file nimcp_multimodal_nlp_bridge.h
 * @brief Bridge connecting sensory cortices to NLP processor
 *
 * WHAT: API for routing multimodal inputs → NLP processing
 * WHY:  Enable language grounding from vision, audio, and speech
 * HOW:  Convert sensory features → token sequences → NLP embeddings
 *
 * ARCHITECTURE:
 * Audio → Speech → Phonemes → Word Tokens → NLP
 * Visual → Features → Text Tokens (OCR) → NLP
 * Speech → Phonemes → Word Tokens → NLP
 *
 * @author Claude Code + NIMCP Development Team
 * @date 2025-11-11
 * @version 2.9.0
 */

#ifndef NIMCP_MULTIMODAL_NLP_BRIDGE_H
#define NIMCP_MULTIMODAL_NLP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#include "nlp/nimcp_nlp.h"
#include "perception/nimcp_speech_cortex.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Speech → NLP Pipeline
//=============================================================================

/**
 * @brief Convert phoneme sequence to word tokens for NLP
 *
 * WHAT: Map phoneme sequence → word IDs → NLP tokens
 * WHY:  Enable speech comprehension via NLP processor
 * HOW:  Lexical access (phonemes → words) → tokenization
 *
 * BIOLOGICAL: Wernicke's area → semantic processing
 *
 * COMPLEXITY: O(P) where P=num_phonemes
 *
 * @param phonemes Phoneme sequence from speech cortex
 * @param num_phonemes Number of phonemes
 * @param tokens Output token IDs [max_tokens]
 * @param max_tokens Maximum tokens to generate
 * @param num_tokens Number of tokens actually generated
 * @return true on success, false on failure
 */
bool multimodal_nlp_phonemes_to_tokens(
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    uint32_t* tokens,
    uint32_t max_tokens,
    uint32_t* num_tokens
);

/**
 * @brief Process speech through NLP processor
 *
 * WHAT: End-to-end speech → NLP pipeline
 * WHY:  Single API for speech language understanding
 * HOW:  Speech features → phonemes → tokens → NLP forward pass
 *
 * PIPELINE:
 * 1. Extract phonemes from speech cortex
 * 2. Convert phonemes to word tokens
 * 3. Process tokens through NLP network
 * 4. Return semantic output
 *
 * COMPLEXITY: O(S + P + T×D) where S=speech, P=phonemes, T=tokens, D=embedding_dim
 *
 * @param speech_cortex Speech cortex instance
 * @param nlp_network NLP processor instance
 * @param audio_data Audio samples
 * @param num_samples Number of audio samples
 * @param output Output semantic vector [output_dim]
 * @param output_dim Output dimensionality
 * @return true on success, false on failure
 */
bool multimodal_nlp_process_speech(
    speech_cortex_t* speech_cortex,
    nlp_network_t nlp_network,
    const float* audio_data,
    uint32_t num_samples,
    float* output,
    uint32_t output_dim
);

//=============================================================================
// Audio → Speech → NLP Pipeline
//=============================================================================

/**
 * @brief Process audio through speech cortex then NLP
 *
 * WHAT: Audio → Speech detection → NLP processing
 * WHY:  Enable audio-based language understanding
 * HOW:  Check speech salience → route to speech cortex → NLP
 *
 * PIPELINE:
 * 1. Process audio through audio cortex
 * 2. Check if audio contains speech
 * 3. If speech detected, route to speech cortex
 * 4. Convert phonemes to tokens
 * 5. Process through NLP
 *
 * COMPLEXITY: O(A + S + P + T×D)
 *
 * @param audio_cortex Audio cortex instance
 * @param speech_cortex Speech cortex instance
 * @param nlp_network NLP processor instance
 * @param audio_data Audio samples
 * @param num_samples Number of audio samples
 * @param num_channels Number of channels (1 or 2)
 * @param output Output semantic vector [output_dim]
 * @param output_dim Output dimensionality
 * @param speech_detected Output: true if speech was detected
 * @return true on success, false on failure
 */
bool multimodal_nlp_process_audio(
    audio_cortex_t* audio_cortex,
    speech_cortex_t* speech_cortex,
    nlp_network_t nlp_network,
    const float* audio_data,
    uint32_t num_samples,
    uint8_t num_channels,
    float* output,
    uint32_t output_dim,
    bool* speech_detected
);

//=============================================================================
// Visual → NLP Pipeline (OCR-like text processing)
//=============================================================================

/**
 * @brief Convert visual features to text token sequence
 *
 * WHAT: Map visual features → text tokens (OCR-like)
 * WHY:  Enable reading text from images
 * HOW:  Pattern matching visual features against learned character/word embeddings
 *
 * NOTE: This is a simplified OCR-like system. Full OCR would require:
 * - Character segmentation
 * - Trained character classifier
 * - Language model for word completion
 *
 * COMPLEXITY: O(F×V) where F=feature_dim, V=vocab_size
 *
 * @param visual_features Visual features from visual cortex
 * @param feature_dim Feature dimensionality
 * @param nlp_network NLP processor (for embedding similarity)
 * @param tokens Output token IDs [max_tokens]
 * @param max_tokens Maximum tokens to generate
 * @param num_tokens Number of tokens actually generated
 * @return true on success, false on failure
 */
bool multimodal_nlp_visual_to_tokens(
    const float* visual_features,
    uint32_t feature_dim,
    nlp_network_t nlp_network,
    uint32_t* tokens,
    uint32_t max_tokens,
    uint32_t* num_tokens
);

/**
 * @brief Process visual input through NLP processor
 *
 * WHAT: End-to-end vision → NLP pipeline
 * WHY:  Enable visual language understanding (text in images)
 * HOW:  Visual features → text tokens → NLP forward pass
 *
 * PIPELINE:
 * 1. Process image through visual cortex
 * 2. Convert visual features to text tokens (OCR-like)
 * 3. Process tokens through NLP network
 * 4. Return semantic output
 *
 * USE CASES:
 * - Reading text in images
 * - Visual question answering
 * - Image captioning (with text prompts)
 *
 * COMPLEXITY: O(W×H + F×V + T×D)
 *
 * @param visual_cortex Visual cortex instance
 * @param nlp_network NLP processor instance
 * @param image Image data
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1 or 3)
 * @param output Output semantic vector [output_dim]
 * @param output_dim Output dimensionality
 * @return true on success, false on failure
 */
bool multimodal_nlp_process_visual(
    visual_cortex_t* visual_cortex,
    nlp_network_t nlp_network,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float* output,
    uint32_t output_dim
);

//=============================================================================
// Multimodal Fusion
//=============================================================================

/**
 * @brief Fuse visual, audio, and text inputs through NLP
 *
 * WHAT: Combine multiple modalities into unified NLP representation
 * WHY:  Enable cross-modal understanding (e.g., "Show me the red ball")
 * HOW:  Process each modality → concatenate embeddings → NLP fusion
 *
 * ARCHITECTURE:
 * Visual features ----\
 * Audio features ------→ Concatenate → NLP fusion layer → Output
 * Text tokens --------/
 *
 * COMPLEXITY: O(V + A + T + (V+A+T)×D)
 *
 * @param visual_cortex Visual cortex (or NULL if no visual input)
 * @param audio_cortex Audio cortex (or NULL if no audio input)
 * @param speech_cortex Speech cortex (or NULL if no speech input)
 * @param nlp_network NLP processor
 * @param image Visual input (or NULL)
 * @param image_width Image width (or 0)
 * @param image_height Image height (or 0)
 * @param image_channels Image channels (or 0)
 * @param audio_data Audio input (or NULL)
 * @param audio_samples Number of audio samples (or 0)
 * @param audio_channels Audio channels (or 0)
 * @param text_tokens Text token input (or NULL)
 * @param num_text_tokens Number of text tokens (or 0)
 * @param output Output fused representation [output_dim]
 * @param output_dim Output dimensionality
 * @return true on success, false on failure
 */
bool multimodal_nlp_fuse_inputs(
    visual_cortex_t* visual_cortex,
    audio_cortex_t* audio_cortex,
    speech_cortex_t* speech_cortex,
    nlp_network_t nlp_network,
    const uint8_t* image,
    uint32_t image_width,
    uint32_t image_height,
    uint32_t image_channels,
    const float* audio_data,
    uint32_t audio_samples,
    uint8_t audio_channels,
    const uint32_t* text_tokens,
    uint32_t num_text_tokens,
    float* output,
    uint32_t output_dim
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if audio contains speech
 *
 * WHAT: Determine if audio input is speech vs noise/music
 * WHY:  Route speech to speech cortex for phoneme extraction
 * HOW:  Check speech salience threshold (energy in 300-3400 Hz)
 *
 * @param audio_cortex Audio cortex instance
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param num_channels Number of channels
 * @param threshold Speech detection threshold [0,1] (default: 0.5)
 * @return true if speech detected, false otherwise
 */
bool multimodal_nlp_is_speech(
    audio_cortex_t* audio_cortex,
    const float* audio_data,
    uint32_t num_samples,
    uint8_t num_channels,
    float threshold
);

/**
 * @brief Check if image contains text
 *
 * WHAT: Determine if visual input contains readable text
 * WHY:  Route text-bearing images to OCR-like processing
 * HOW:  Check for text-like edge patterns (horizontal/vertical lines)
 *
 * @param visual_cortex Visual cortex instance
 * @param image Image data
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels
 * @param threshold Text detection threshold [0,1] (default: 0.5)
 * @return true if text detected, false otherwise
 */
bool multimodal_nlp_contains_text(
    visual_cortex_t* visual_cortex,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float threshold
);

/**
 * @brief Get default phoneme-to-token mapping
 *
 * WHAT: Initialize phoneme → token lookup table
 * WHY:  Bootstrap lexical access for speech processing
 * HOW:  Load common English words with phoneme sequences
 *
 * @return true on success, false on failure
 */
bool multimodal_nlp_init_phoneme_lexicon(void);

/**
 * @brief Clean up phoneme-to-token resources
 */
void multimodal_nlp_cleanup_phoneme_lexicon(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MULTIMODAL_NLP_BRIDGE_H
