//=============================================================================
// nimcp_lateralization.c - Hemisphere Specialization Implementation
//=============================================================================
/**
 * @file nimcp_lateralization.c
 * @brief Implementation of cognitive domain lateralization and hemisphere
 *        specialization profiles
 *
 * BIOLOGICAL BASIS:
 * - Language lateralization: 95% left hemisphere for right-handers
 * - Spatial processing: 80% right hemisphere dominant
 * - Handedness affects motor but also language lateralization
 * - Left-handers show more bilateral and variable patterns
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lateralization, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Default Profiles
//=============================================================================

/**
 * @brief Get default lateralization profile (right-handed)
 *
 * WHAT: Standard lateralization for typical right-handed brain
 * WHY:  Provides biologically accurate defaults for ~90% of population
 * HOW:  Based on neuroimaging studies of hemisphere specialization
 */
lateralization_profile_t lateralization_default_profile(void) {
    lateralization_profile_t profile = {
        // Domain weights [0.0 = right dominant, 1.0 = left dominant]
        .language_dominance = 0.95f,            // Strongly left
        .spatial_dominance = 0.20f,             // Right dominant
        .motor_fine_dominance = 0.90f,          // Left (controls right hand)
        .motor_gross_dominance = 0.50f,         // Bilateral
        .emotion_processing_dominance = 0.30f,  // Right dominant
        .attention_global_dominance = 0.25f,    // Right (forest)
        .attention_local_dominance = 0.75f,     // Left (trees)
        .music_melody_dominance = 0.20f,        // Right dominant
        .music_rhythm_dominance = 0.80f,        // Left dominant
        .face_recognition_dominance = 0.15f,    // Strongly right
        .logical_reasoning_dominance = 0.85f,   // Left dominant
        .creative_thinking_dominance = 0.35f,   // Right-leaning

        // Handedness
        .handedness = HANDEDNESS_RIGHT,

        // Plasticity configuration
        .enable_plasticity = true,
        .plasticity_rate = 0.001f,              // Slow shift
        .min_dominance = 0.05f,                 // Never fully lateralized
        .max_dominance = 0.95f                  // Keep some bilateral capacity
    };
    return profile;
}

/**
 * @brief Get left-handed lateralization profile
 *
 * WHAT: Adjusted lateralization for left-handed individuals
 * WHY:  Left-handers show more bilateral and variable lateralization
 * HOW:  Motor reversed, language slightly more bilateral
 */
lateralization_profile_t lateralization_left_handed_profile(void) {
    lateralization_profile_t profile = {
        // Left-handers: motor reversed, language more bilateral
        .language_dominance = 0.70f,            // More bilateral than right-handers
        .spatial_dominance = 0.30f,             // Slightly more bilateral
        .motor_fine_dominance = 0.10f,          // Right hemisphere (left hand)
        .motor_gross_dominance = 0.50f,         // Still bilateral
        .emotion_processing_dominance = 0.35f,  // Slightly more bilateral
        .attention_global_dominance = 0.30f,    // Slightly more bilateral
        .attention_local_dominance = 0.70f,     // Slightly more bilateral
        .music_melody_dominance = 0.25f,        // Slightly more bilateral
        .music_rhythm_dominance = 0.75f,        // Slightly more bilateral
        .face_recognition_dominance = 0.20f,    // Slightly more bilateral
        .logical_reasoning_dominance = 0.80f,   // Slightly more bilateral
        .creative_thinking_dominance = 0.40f,   // Slightly more bilateral

        .handedness = HANDEDNESS_LEFT,

        .enable_plasticity = true,
        .plasticity_rate = 0.002f,              // Higher plasticity
        .min_dominance = 0.05f,
        .max_dominance = 0.95f
    };
    return profile;
}

/**
 * @brief Get bilateral (ambidextrous) profile
 *
 * WHAT: Symmetric lateralization across hemispheres
 * WHY:  For experiments requiring balanced processing
 * HOW:  All domains at 0.5 dominance
 */
lateralization_profile_t lateralization_bilateral_profile(void) {
    lateralization_profile_t profile = {
        // All bilateral
        .language_dominance = 0.50f,
        .spatial_dominance = 0.50f,
        .motor_fine_dominance = 0.50f,
        .motor_gross_dominance = 0.50f,
        .emotion_processing_dominance = 0.50f,
        .attention_global_dominance = 0.50f,
        .attention_local_dominance = 0.50f,
        .music_melody_dominance = 0.50f,
        .music_rhythm_dominance = 0.50f,
        .face_recognition_dominance = 0.50f,
        .logical_reasoning_dominance = 0.50f,
        .creative_thinking_dominance = 0.50f,

        .handedness = HANDEDNESS_AMBIDEXTROUS,

        .enable_plasticity = true,
        .plasticity_rate = 0.003f,              // Most plastic
        .min_dominance = 0.05f,
        .max_dominance = 0.95f
    };
    return profile;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get dominance value for a cognitive domain
 *
 * WHAT: Retrieve the lateralization weight for a specific domain
 * WHY:  Used to route processing to appropriate hemisphere
 * HOW:  Switch on domain type, return corresponding weight
 */
float lateralization_get_dominance(
    const lateralization_profile_t* profile,
    cognitive_domain_t domain
) {
    if (!profile) {
        return 0.5f;  // Default to bilateral
    }

    switch (domain) {
        case COGNITIVE_DOMAIN_LANGUAGE:
            return profile->language_dominance;
        case COGNITIVE_DOMAIN_SPATIAL:
            return profile->spatial_dominance;
        case COGNITIVE_DOMAIN_MOTOR_FINE:
            return profile->motor_fine_dominance;
        case COGNITIVE_DOMAIN_MOTOR_GROSS:
            return profile->motor_gross_dominance;
        case COGNITIVE_DOMAIN_EMOTION:
            return profile->emotion_processing_dominance;
        case COGNITIVE_DOMAIN_ATTENTION_GLOBAL:
            return profile->attention_global_dominance;
        case COGNITIVE_DOMAIN_ATTENTION_LOCAL:
            return profile->attention_local_dominance;
        case COGNITIVE_DOMAIN_MUSIC_MELODY:
            return profile->music_melody_dominance;
        case COGNITIVE_DOMAIN_MUSIC_RHYTHM:
            return profile->music_rhythm_dominance;
        case COGNITIVE_DOMAIN_FACE_RECOGNITION:
            return profile->face_recognition_dominance;
        case COGNITIVE_DOMAIN_LOGICAL_REASONING:
            return profile->logical_reasoning_dominance;
        case COGNITIVE_DOMAIN_CREATIVE_THINKING:
            return profile->creative_thinking_dominance;
        default:
            return 0.5f;  // Unknown domain = bilateral
    }
}

/**
 * @brief Get dominant hemisphere for a domain
 *
 * WHAT: Determine which hemisphere is dominant for a cognitive domain
 * WHY:  Quick lookup for routing decisions
 * HOW:  Compare dominance to 0.5 threshold
 */
hemisphere_id_t lateralization_get_dominant_hemisphere(
    const lateralization_profile_t* profile,
    cognitive_domain_t domain
) {
    float dominance = lateralization_get_dominance(profile, domain);
    return (dominance > 0.5f) ? HEMISPHERE_LEFT : HEMISPHERE_RIGHT;
}

/**
 * @brief Check if domain is strongly lateralized
 *
 * WHAT: Determine if domain shows clear hemisphere preference
 * WHY:  Strong lateralization (>0.7 or <0.3) suggests specialized routing
 * HOW:  Compare dominance against bilateral thresholds
 */
bool lateralization_is_strongly_lateralized(
    const lateralization_profile_t* profile,
    cognitive_domain_t domain
) {
    float dominance = lateralization_get_dominance(profile, domain);
    return (dominance > 0.7f) || (dominance < 0.3f);
}

//=============================================================================
// Plasticity Functions
//=============================================================================

/**
 * @brief Internal helper to set dominance value by domain
 */
static int set_dominance_value(
    lateralization_profile_t* profile,
    cognitive_domain_t domain,
    float value
) {
    switch (domain) {
        case COGNITIVE_DOMAIN_LANGUAGE:
            profile->language_dominance = value;
            break;
        case COGNITIVE_DOMAIN_SPATIAL:
            profile->spatial_dominance = value;
            break;
        case COGNITIVE_DOMAIN_MOTOR_FINE:
            profile->motor_fine_dominance = value;
            break;
        case COGNITIVE_DOMAIN_MOTOR_GROSS:
            profile->motor_gross_dominance = value;
            break;
        case COGNITIVE_DOMAIN_EMOTION:
            profile->emotion_processing_dominance = value;
            break;
        case COGNITIVE_DOMAIN_ATTENTION_GLOBAL:
            profile->attention_global_dominance = value;
            break;
        case COGNITIVE_DOMAIN_ATTENTION_LOCAL:
            profile->attention_local_dominance = value;
            break;
        case COGNITIVE_DOMAIN_MUSIC_MELODY:
            profile->music_melody_dominance = value;
            break;
        case COGNITIVE_DOMAIN_MUSIC_RHYTHM:
            profile->music_rhythm_dominance = value;
            break;
        case COGNITIVE_DOMAIN_FACE_RECOGNITION:
            profile->face_recognition_dominance = value;
            break;
        case COGNITIVE_DOMAIN_LOGICAL_REASONING:
            profile->logical_reasoning_dominance = value;
            break;
        case COGNITIVE_DOMAIN_CREATIVE_THINKING:
            profile->creative_thinking_dominance = value;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "set_dominance_value: operation failed");
            return -1;
    }
    return 0;
}

/**
 * @brief Shift dominance for a domain (plasticity)
 *
 * WHAT: Adjust hemisphere dominance based on usage/training
 * WHY:  Models neuroplasticity and recovery from injury
 * HOW:  Shift clamped to [min_dominance, max_dominance]
 */
int lateralization_shift_dominance(
    lateralization_profile_t* profile,
    cognitive_domain_t domain,
    float shift
) {
    if (!profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "profile is NULL");

        return -1;
    }

    if (!profile->enable_plasticity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "lateralization_shift_dominance: plasticity is disabled");
        return -1;  // Plasticity disabled
    }

    if (domain >= COGNITIVE_DOMAIN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "lateralization_shift_dominance: invalid domain");
        return -1;  // Invalid domain
    }

    // Get current value
    float current = lateralization_get_dominance(profile, domain);

    // Apply shift
    float new_value = current + shift;

    // Clamp to valid range
    if (new_value < profile->min_dominance) {
        new_value = profile->min_dominance;
    }
    if (new_value > profile->max_dominance) {
        new_value = profile->max_dominance;
    }

    // Set new value
    return set_dominance_value(profile, domain, new_value);
}

/**
 * @brief Apply usage-based plasticity
 *
 * WHAT: Strengthen hemisphere that was just activated
 * WHY:  "Neurons that fire together wire together"
 * HOW:  Small shift toward activated hemisphere
 */
int lateralization_apply_usage_plasticity(
    lateralization_profile_t* profile,
    cognitive_domain_t domain,
    hemisphere_id_t hemisphere
) {
    if (!profile || !profile->enable_plasticity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "lateralization_apply_usage_plasticity: profile is NULL or plasticity disabled");
        return -1;
    }

    if (domain >= COGNITIVE_DOMAIN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "lateralization_apply_usage_plasticity: invalid domain");
        return -1;
    }

    // Shift toward the active hemisphere
    // Left = positive shift (toward 1.0)
    // Right = negative shift (toward 0.0)
    float shift = (hemisphere == HEMISPHERE_LEFT)
        ? profile->plasticity_rate
        : -profile->plasticity_rate;

    return lateralization_shift_dominance(profile, domain, shift);
}

/**
 * @brief Reset lateralization to default
 */
void lateralization_reset(
    lateralization_profile_t* profile,
    handedness_t handedness
) {
    if (!profile) {
        return;
    }

    lateralization_profile_t default_profile;

    switch (handedness) {
        case HANDEDNESS_LEFT:
            default_profile = lateralization_left_handed_profile();
            break;
        case HANDEDNESS_AMBIDEXTROUS:
            default_profile = lateralization_bilateral_profile();
            break;
        case HANDEDNESS_RIGHT:
        default:
            default_profile = lateralization_default_profile();
            break;
    }

    memcpy(profile, &default_profile, sizeof(lateralization_profile_t));
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get string name for cognitive domain
 */
const char* cognitive_domain_name(cognitive_domain_t domain) {
    switch (domain) {
        case COGNITIVE_DOMAIN_LANGUAGE:
            return "Language";
        case COGNITIVE_DOMAIN_SPATIAL:
            return "Spatial";
        case COGNITIVE_DOMAIN_MOTOR_FINE:
            return "Fine Motor";
        case COGNITIVE_DOMAIN_MOTOR_GROSS:
            return "Gross Motor";
        case COGNITIVE_DOMAIN_EMOTION:
            return "Emotion Processing";
        case COGNITIVE_DOMAIN_ATTENTION_GLOBAL:
            return "Global Attention";
        case COGNITIVE_DOMAIN_ATTENTION_LOCAL:
            return "Local Attention";
        case COGNITIVE_DOMAIN_MUSIC_MELODY:
            return "Music Melody";
        case COGNITIVE_DOMAIN_MUSIC_RHYTHM:
            return "Music Rhythm";
        case COGNITIVE_DOMAIN_FACE_RECOGNITION:
            return "Face Recognition";
        case COGNITIVE_DOMAIN_LOGICAL_REASONING:
            return "Logical Reasoning";
        case COGNITIVE_DOMAIN_CREATIVE_THINKING:
            return "Creative Thinking";
        default:
            return "Unknown";
    }
}

/**
 * @brief Get string name for hemisphere
 */
const char* hemisphere_name(hemisphere_id_t hemisphere) {
    switch (hemisphere) {
        case HEMISPHERE_LEFT:
            return "Left";
        case HEMISPHERE_RIGHT:
            return "Right";
        default:
            return "Unknown";
    }
}

/**
 * @brief Get string name for handedness
 */
const char* handedness_name(handedness_t handedness) {
    switch (handedness) {
        case HANDEDNESS_RIGHT:
            return "Right-handed";
        case HANDEDNESS_LEFT:
            return "Left-handed";
        case HANDEDNESS_AMBIDEXTROUS:
            return "Ambidextrous";
        default:
            return "Unknown";
    }
}

/**
 * @brief Validate lateralization profile
 */
bool lateralization_validate(const lateralization_profile_t* profile) {
    if (!profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lateralization_validate: profile is NULL");
        return false;
    }

    // Check all dominance values are in valid range [0.0, 1.0]
    if (profile->language_dominance < 0.0f || profile->language_dominance > 1.0f) {
        return false;
    }
    if (profile->spatial_dominance < 0.0f || profile->spatial_dominance > 1.0f) {
        return false;
    }
    if (profile->motor_fine_dominance < 0.0f || profile->motor_fine_dominance > 1.0f) {
        return false;
    }
    if (profile->motor_gross_dominance < 0.0f || profile->motor_gross_dominance > 1.0f) {
        return false;
    }
    if (profile->emotion_processing_dominance < 0.0f ||
        profile->emotion_processing_dominance > 1.0f) {
        return false;
    }
    if (profile->attention_global_dominance < 0.0f ||
        profile->attention_global_dominance > 1.0f) {
        return false;
    }
    if (profile->attention_local_dominance < 0.0f ||
        profile->attention_local_dominance > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lateralization_validate: operation failed");
        return false;
    }
    if (profile->music_melody_dominance < 0.0f || profile->music_melody_dominance > 1.0f) {
        return false;
    }
    if (profile->music_rhythm_dominance < 0.0f || profile->music_rhythm_dominance > 1.0f) {
        return false;
    }
    if (profile->face_recognition_dominance < 0.0f ||
        profile->face_recognition_dominance > 1.0f) {
        return false;
    }
    if (profile->logical_reasoning_dominance < 0.0f ||
        profile->logical_reasoning_dominance > 1.0f) {
        return false;
    }
    if (profile->creative_thinking_dominance < 0.0f ||
        profile->creative_thinking_dominance > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lateralization_validate: operation failed");
        return false;
    }

    // Check plasticity configuration
    if (profile->plasticity_rate < 0.0f) {
        return false;
    }
    if (profile->min_dominance < 0.0f || profile->min_dominance > 1.0f) {
        return false;
    }
    if (profile->max_dominance < 0.0f || profile->max_dominance > 1.0f) {
        return false;
    }
    if (profile->min_dominance >= profile->max_dominance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "lateralization_validate: capacity exceeded");
        return false;
    }

    // Check handedness
    if (profile->handedness > HANDEDNESS_AMBIDEXTROUS) {
        return false;
    }

    return true;
}
