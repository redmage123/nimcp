//=============================================================================
// sensory_extractor.c - Sensory Feature Extraction Implementation
//=============================================================================
/**
 * @file sensory_extractor.c
 * @brief Single Responsibility: Extract features from raw sensory inputs
 *
 * REFACTORING NOTE:
 * Extracted from nimcp_brain.c brain_process_multimodal() (394 lines → ~60 lines)
 * Reason: Apply Single Responsibility Principle - separate sensory processing
 *
 * DESIGN:
 * - Pure function: no side effects except buffer writes
 * - Testable: can mock cortices and test feature extraction independently
 * - Reusable: can be used by any module needing sensory features
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"

#include "core/brain/processing/sensory_extractor.h"
#include "core/brain/nimcp_brain.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "BRAIN_PROC_SENS"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(sensory_extractor, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Brain Structure Access
//=============================================================================

// Note: brain_struct is opaque - we only access it through getter functions
// in the actual implementation. For now, stub implementations will be used.

//=============================================================================
// API Implementation
//=============================================================================

bool sensory_input_validate(const sensory_input_t* input)
{
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "sensory_input_validate: input is NULL");

            return false;
    }

    bool has_visual = (input->visual_data != NULL &&
                      input->visual_width > 0 &&
                      input->visual_height > 0);

    bool has_audio = (input->audio_data != NULL &&
                     input->audio_samples > 0);

    bool has_direct = (input->direct_data != NULL &&
                      input->direct_dim > 0);

    return (has_visual || has_audio || has_direct);
}

void sensory_features_init(sensory_features_t* features)
{
    if (!features) {
        return;
    }

    memset(features, 0, sizeof(sensory_features_t));

    features->visual_valid = false;
    features->audio_valid = false;
    features->speech_valid = false;
    features->direct_valid = false;
}

bool sensory_extract_features(
    const brain_t brain,
    const sensory_input_t* input,
    sensory_features_t* features)
{
    // =========================================================================
    // VALIDATION
    // =========================================================================

    if (!brain || !input || !features) {
        fprintf(stderr, "sensory_extractor: Invalid parameters\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_extract_features: required parameter is NULL (brain, input, features)");
        return false;
    }

    if (!sensory_input_validate(input)) {
        fprintf(stderr, "sensory_extractor: No valid sensory input\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sensory_extract_features: sensory_input_validate is NULL");
        return false;
    }

    // Initialize output
    sensory_features_init(features);

    // =========================================================================
    // VISUAL FEATURE EXTRACTION (V1 Cortex)
    // =========================================================================

    bool has_visual = (input->visual_data != NULL &&
                      input->visual_width > 0 &&
                      input->visual_height > 0);

    if (has_visual && brain->visual_cortex && brain->visual_feature_buffer) {
        bool visual_success = visual_cortex_process(
            brain->visual_cortex,
            input->visual_data,
            input->visual_width,
            input->visual_height,
            input->visual_channels,
            brain->visual_feature_buffer
        );

        if (visual_success) {
            features->visual_features = brain->visual_feature_buffer;
            features->visual_dim = brain->config.visual_feature_dim;
            features->visual_valid = true;
        }
    }

    // =========================================================================
    // AUDIO FEATURE EXTRACTION (A1 Cortex)
    // =========================================================================

    bool has_audio = (input->audio_data != NULL &&
                     input->audio_samples > 0);

    bool audio_success = false;

    if (has_audio && brain->audio_cortex && brain->audio_feature_buffer) {
        audio_success = audio_cortex_process(
            brain->audio_cortex,
            input->audio_data,
            input->audio_samples,
            input->audio_channels,
            brain->audio_feature_buffer
        );

        if (audio_success) {
            features->audio_features = brain->audio_feature_buffer;
            features->audio_dim = brain->config.audio_feature_dim;
            features->audio_valid = true;
        }
    }

    // =========================================================================
    // SPEECH FEATURE EXTRACTION (STG/Wernicke Cortex)
    // =========================================================================

    // Hierarchical processing: A1 → STG/Wernicke
    // Speech cortex takes audio cortex output as input
    if (has_audio && audio_success &&
        brain->speech_cortex && brain->speech_feature_buffer) {

        bool speech_success = speech_cortex_process(
            brain->speech_cortex,
            input->audio_data,
            input->audio_samples,
            brain->speech_feature_buffer
        );

        if (speech_success) {
            features->speech_features = brain->speech_feature_buffer;
            features->speech_dim = brain->config.speech_feature_dim;
            features->speech_valid = true;
        }
    }

    // =========================================================================
    // DIRECT FEATURES (Pass-through)
    // =========================================================================

    bool has_direct = (input->direct_data != NULL &&
                      input->direct_dim > 0);

    if (has_direct) {
        features->direct_features = input->direct_data;
        features->direct_dim = input->direct_dim;
        features->direct_valid = true;
    }

    // =========================================================================
    // SUCCESS CHECK
    // =========================================================================

    // At least one modality must succeed
    bool any_valid = (features->visual_valid ||
                     features->audio_valid ||
                     features->speech_valid ||
                     features->direct_valid);

    if (!any_valid) {
        fprintf(stderr, "sensory_extractor: No features extracted from any modality\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensory_extract_features: any_valid is NULL");
        return false;
    }

    return true;
}
