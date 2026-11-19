#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_multimodal_nlp_bridge.c - Multimodal-to-NLP Integration Implementation
//=============================================================================

#include "nlp/nimcp_multimodal_nlp_bridge.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "MULTIMODAL_NLP"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(multimodal_nlp_bridge)

//=============================================================================
// Phoneme Lexicon (Simple Mapping)
//=============================================================================

// Simplified phoneme → word → token mapping
// In production, this would be a full lexicon with thousands of entries
typedef struct {
    phoneme_t phonemes[10];  // Max 10 phonemes per word
    uint8_t num_phonemes;
    uint32_t token_id;
    char word[32];
} phoneme_word_entry_t;

static phoneme_word_entry_t* g_phoneme_lexicon = NULL;
static uint32_t g_lexicon_size = 0;

bool multimodal_nlp_init_phoneme_lexicon(void) {
    LOG_DEBUG(LOG_MODULE, "Initializing phoneme lexicon");

    // Simplified lexicon with a few common words
    // In production, this would load from a file/database

    static phoneme_word_entry_t lexicon[] = {
        // "cat" = /k/ /ae/ /t/
        {{PHONEME_K, PHONEME_AE, PHONEME_T}, 3, 100, "cat"},
        // "dog" = /d/ /ao/ /g/
        {{PHONEME_D, PHONEME_AO, PHONEME_G}, 3, 101, "dog"},
        // "hello" = /h/ /eh/ /l/ /ow/
        {{PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW}, 4, 102, "hello"},
        // "yes" = /y/ /eh/ /s/
        {{PHONEME_Y, PHONEME_EH, PHONEME_S}, 3, 103, "yes"},
        // "no" = /n/ /ow/
        {{PHONEME_N, PHONEME_OW}, 2, 104, "no"},
    };

    g_lexicon_size = sizeof(lexicon) / sizeof(phoneme_word_entry_t);
    g_phoneme_lexicon = (phoneme_word_entry_t*)nimcp_malloc(sizeof(lexicon));
    if (!g_phoneme_lexicon) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multimodal_nlp_init_phoneme_lexicon: Failed to allocate phoneme lexicon");
        LOG_ERROR(LOG_MODULE, "Failed to allocate phoneme lexicon");
        return false;
    }

    memcpy(g_phoneme_lexicon, lexicon, sizeof(lexicon));
    LOG_INFO(LOG_MODULE, "Phoneme lexicon initialized with %u entries", g_lexicon_size);
    return true;
}

void multimodal_nlp_cleanup_phoneme_lexicon(void) {
    if (g_phoneme_lexicon) {
        nimcp_free(g_phoneme_lexicon);
        g_phoneme_lexicon = NULL;
        g_lexicon_size = 0;
        LOG_DEBUG(LOG_MODULE, "Phoneme lexicon cleaned up");
    }
}

//=============================================================================
// Phoneme → Token Conversion
//=============================================================================

static bool phoneme_sequence_matches(
    const phoneme_t* seq1,
    uint32_t len1,
    const phoneme_t* seq2,
    uint32_t len2
) {
    if (len1 != len2) {
        return false;  /* Different lengths - normal mismatch */
    }
    for (uint32_t i = 0; i < len1; i++) {
        if (seq1[i] != seq2[i]) {
            return false;  /* Phoneme mismatch - normal comparison result */
        }
    }
    return true;
}

bool multimodal_nlp_phonemes_to_tokens(
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    uint32_t* tokens,
    uint32_t max_tokens,
    uint32_t* num_tokens
) {
    if (!phonemes || !tokens || !num_tokens || max_tokens == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_phonemes_to_tokens: Invalid parameters");
        LOG_ERROR(LOG_MODULE, "phonemes_to_tokens: Invalid parameters");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Converting %u phonemes to tokens", num_phonemes);

    // Initialize lexicon if not already done
    if (!g_phoneme_lexicon) {
        if (!multimodal_nlp_init_phoneme_lexicon()) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "multimodal_nlp_phonemes_to_tokens: Failed to initialize phoneme lexicon");
            LOG_ERROR(LOG_MODULE, "Failed to initialize phoneme lexicon");
            return false;
        }
    }

    *num_tokens = 0;
    uint32_t pos = 0;

    // Greedy longest-match phoneme → word conversion
    while (pos < num_phonemes && *num_tokens < max_tokens) {
        bool found = false;

        // Try to match longest phoneme sequence first
        for (int32_t len = 10; len >= 1; len--) {
            if (pos + len > num_phonemes) continue;

            // Check all lexicon entries
            for (uint32_t i = 0; i < g_lexicon_size; i++) {
                if (phoneme_sequence_matches(
                    &phonemes[pos], len,
                    g_phoneme_lexicon[i].phonemes,
                    g_phoneme_lexicon[i].num_phonemes
                )) {
                    tokens[*num_tokens] = g_phoneme_lexicon[i].token_id;
                    (*num_tokens)++;
                    pos += len;
                    found = true;
                    break;
                }
            }

            if (found) break;
        }

        if (!found) {
            // Unknown phoneme sequence - skip one phoneme
            LOG_DEBUG(LOG_MODULE, "Unknown phoneme at position %u, skipping", pos);
            pos++;
        }
    }

    LOG_DEBUG(LOG_MODULE, "Converted %u phonemes to %u tokens", num_phonemes, *num_tokens);
    return true;
}

//=============================================================================
// Speech → NLP Pipeline
//=============================================================================

bool multimodal_nlp_process_speech(
    speech_cortex_t* speech_cortex,
    nlp_network_t nlp_network,
    const float* audio_data,
    uint32_t num_samples,
    float* output,
    uint32_t output_dim
) {
    if (!speech_cortex || !nlp_network || !audio_data || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_process_speech: Invalid parameters");
        LOG_ERROR(LOG_MODULE, "process_speech: Invalid parameters");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Processing speech: %u samples, output_dim=%u", num_samples, output_dim);

    // Step 1: Detect phonemes from audio
    phoneme_event_t phoneme_events[50];
    uint32_t num_detected = 0;

    if (!speech_cortex_detect_phonemes(
        speech_cortex,
        audio_data,
        num_samples,
        phoneme_events,
        50,
        &num_detected
    )) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "multimodal_nlp_process_speech: Failed to detect phonemes");
        LOG_ERROR(LOG_MODULE, "Failed to detect phonemes");
        return false;
    }

    if (num_detected == 0) {
        // No phonemes detected - return zero vector
        LOG_DEBUG(LOG_MODULE, "No phonemes detected in audio");
        memset(output, 0, output_dim * sizeof(float));
        return true;
    }

    LOG_DEBUG(LOG_MODULE, "Detected %u phonemes", num_detected);

    // Step 2: Extract phoneme sequence
    phoneme_t phonemes[50];
    for (uint32_t i = 0; i < num_detected && i < 50; i++) {
        phonemes[i] = phoneme_events[i].phoneme;
    }

    // Step 3: Convert phonemes to word tokens
    uint32_t tokens[20];
    uint32_t num_tokens = 0;

    if (!multimodal_nlp_phonemes_to_tokens(
        phonemes,
        num_detected,
        tokens,
        20,
        &num_tokens
    )) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_nlp_process_speech: operation failed");
        return false;
    }

    if (num_tokens == 0) {
        // No tokens generated - return zero vector
        memset(output, 0, output_dim * sizeof(float));
        return true;
    }

    // Step 4: Process tokens through NLP network
    if (!nlp_network_forward(
        nlp_network,
        tokens,
        num_tokens,
        output,
        output_dim
    )) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_nlp_process_speech: operation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Audio → Speech → NLP Pipeline
//=============================================================================

bool multimodal_nlp_is_speech(
    audio_cortex_t* audio_cortex,
    const float* audio_data,
    uint32_t num_samples,
    uint8_t num_channels,
    float threshold
) {
    if (!audio_cortex || !audio_data) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_is_speech: NULL audio_cortex or audio_data");
        return false;
    }

    // Process audio to extract features
    float features[128];
    if (!audio_cortex_process(audio_cortex, audio_data, num_samples, num_channels, features)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_nlp_is_speech: audio_cortex_process is NULL");
        return false;
    }

    // Check speech salience
    float speech_salience = audio_cortex_get_speech_salience(audio_cortex, features, 128);

    return speech_salience >= threshold;
}

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
) {
    if (!audio_cortex || !speech_cortex || !nlp_network || !audio_data || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_process_audio: NULL parameter");
        return false;
    }

    // Step 1: Check if audio contains speech
    bool is_speech = multimodal_nlp_is_speech(audio_cortex, audio_data, num_samples, num_channels, 0.5F);

    if (speech_detected) {
        *speech_detected = is_speech;
    }

    if (!is_speech) {
        // Not speech - return zero vector
        memset(output, 0, output_dim * sizeof(float));
        return true;
    }

    // Step 2: Activate speech processing mode
    audio_cortex_activate_speech_mode(audio_cortex);

    // Step 3: Process through speech cortex → NLP
    return multimodal_nlp_process_speech(
        speech_cortex,
        nlp_network,
        audio_data,
        num_samples,
        output,
        output_dim
    );
}

//=============================================================================
// Visual → NLP Pipeline (OCR-like)
//=============================================================================

bool multimodal_nlp_visual_to_tokens(
    const float* visual_features,
    uint32_t feature_dim,
    nlp_network_t nlp_network,
    uint32_t* tokens,
    uint32_t max_tokens,
    uint32_t* num_tokens
) {
    if (!visual_features || !nlp_network || !tokens || !num_tokens) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_visual_to_tokens: NULL parameter");
        return false;
    }

    // Simplified OCR-like token generation:
    // Compare visual features to learned embeddings via cosine similarity
    // In production, this would use a trained character/word classifier

    *num_tokens = 0;

    // Try to match visual features to word embeddings
    float best_similarity = -1.0F;
    uint32_t best_token = 0;

    // Search through vocabulary (simplified: check first 1000 tokens)
    for (uint32_t token_id = 0; token_id < 1000 && token_id < max_tokens; token_id++) {
        float embedding[128];
        if (!nlp_network_get_embedding(nlp_network, token_id, embedding)) {
            continue;
        }

        // Compute cosine similarity
        float dot = 0.0F, norm1 = 0.0F, norm2 = 0.0F;
        uint32_t min_dim = (feature_dim < 128) ? feature_dim : 128;

        for (uint32_t i = 0; i < min_dim; i++) {
            dot += visual_features[i] * embedding[i];
            norm1 += visual_features[i] * visual_features[i];
            norm2 += embedding[i] * embedding[i];
        }

        float similarity = dot / (sqrtf(norm1) * sqrtf(norm2) + 1e-8F);

        if (similarity > best_similarity) {
            best_similarity = similarity;
            best_token = token_id;
        }
    }

    // If similarity is above threshold, use this token
    if (best_similarity > 0.5F && *num_tokens < max_tokens) {
        tokens[*num_tokens] = best_token;
        (*num_tokens)++;
    }

    return true;
}

bool multimodal_nlp_contains_text(
    visual_cortex_t* visual_cortex,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float threshold
) {
    if (!visual_cortex || !image) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_contains_text: NULL parameter");
        return false;
    }

    // Simplified text detection:
    // Extract visual features and check for text-like patterns
    // (horizontal/vertical edges at text scales)

    float features[128];
    if (!visual_cortex_process(visual_cortex, image, width, height, channels, features)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_nlp_contains_text: visual_cortex_process is NULL");
        return false;
    }

    // Check if features indicate text-like patterns
    // In production, this would use a trained text detector
    // For now, we use a simple heuristic: high edge density in mid frequencies

    float edge_energy = 0.0F;
    for (uint32_t i = 20; i < 60 && i < 128; i++) {
        edge_energy += fabsf(features[i]);
    }
    edge_energy /= 40.0F;

    return edge_energy >= threshold;
}

bool multimodal_nlp_process_visual(
    visual_cortex_t* visual_cortex,
    nlp_network_t nlp_network,
    const uint8_t* image,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    float* output,
    uint32_t output_dim
) {
    if (!visual_cortex || !nlp_network || !image || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_process_visual: NULL parameter");
        return false;
    }

    // Step 1: Extract visual features
    float features[128];
    if (!visual_cortex_process(visual_cortex, image, width, height, channels, features)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_nlp_process_visual: visual_cortex_process is NULL");
        return false;
    }

    // Step 2: Convert visual features to text tokens (OCR-like)
    uint32_t tokens[20];
    uint32_t num_tokens = 0;

    if (!multimodal_nlp_visual_to_tokens(features, 128, nlp_network, tokens, 20, &num_tokens)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_nlp_process_visual: multimodal_nlp_visual_to_tokens is NULL");
        return false;
    }

    if (num_tokens == 0) {
        // No tokens generated (no text detected) - return zero vector
        memset(output, 0, output_dim * sizeof(float));
        return true;
    }

    // Step 3: Process tokens through NLP network
    if (!nlp_network_forward(nlp_network, tokens, num_tokens, output, output_dim)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multimodal_nlp_process_visual: nlp_network_forward is NULL");
        return false;
    }

    return true;
}

//=============================================================================
// Multimodal Fusion
//=============================================================================

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
) {
    if (!nlp_network || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multimodal_nlp_fuse_inputs: Invalid parameters");
        LOG_ERROR(LOG_MODULE, "fuse_inputs: Invalid parameters");
        return false;
    }

    LOG_INFO(LOG_MODULE, "Fusing multimodal inputs: visual=%d, audio=%d, text=%d",
             image != NULL, audio_data != NULL, text_tokens != NULL);

    // Allocate buffers for each modality
    float visual_output[128] = {0};
    float audio_output[128] = {0};
    float text_output[128] = {0};

    bool has_visual = false, has_audio = false, has_text = false;

    // Process visual input if provided
    if (visual_cortex && image) {
        has_visual = multimodal_nlp_process_visual(
            visual_cortex,
            nlp_network,
            image,
            image_width,
            image_height,
            image_channels,
            visual_output,
            128
        );
    }

    // Process audio input if provided
    if (audio_cortex && speech_cortex && audio_data) {
        bool speech_detected = false;
        has_audio = multimodal_nlp_process_audio(
            audio_cortex,
            speech_cortex,
            nlp_network,
            audio_data,
            audio_samples,
            audio_channels,
            audio_output,
            128,
            &speech_detected
        );
    }

    // Process text input if provided
    if (text_tokens && num_text_tokens > 0) {
        has_text = nlp_network_forward(
            nlp_network,
            text_tokens,
            num_text_tokens,
            text_output,
            128
        );
    }

    // Fuse modalities by averaging (simple fusion strategy)
    // In production, this would use learned fusion weights or attention
    memset(output, 0, output_dim * sizeof(float));

    uint32_t num_modalities = 0;
    if (has_visual) num_modalities++;
    if (has_audio) num_modalities++;
    if (has_text) num_modalities++;

    if (num_modalities == 0) {
        LOG_WARN(LOG_MODULE, "No modalities processed successfully");
        return true;  // Return zero vector (no modalities processed)
    }

    LOG_DEBUG(LOG_MODULE, "Fusing %u modalities", num_modalities);

    uint32_t fuse_dim = (output_dim < 128) ? output_dim : 128;

    for (uint32_t i = 0; i < fuse_dim; i++) {
        float sum = 0.0F;
        if (has_visual) sum += visual_output[i];
        if (has_audio) sum += audio_output[i];
        if (has_text) sum += text_output[i];
        output[i] = sum / (float)num_modalities;
    }

    LOG_INFO(LOG_MODULE, "Multimodal fusion completed successfully");
    return true;
}
