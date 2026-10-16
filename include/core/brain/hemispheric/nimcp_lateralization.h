//=============================================================================
// nimcp_lateralization.h - Hemisphere Specialization Configuration
//=============================================================================
/**
 * @file nimcp_lateralization.h
 * @brief Cognitive domain lateralization and hemisphere specialization
 *
 * WHAT: Configures hemisphere dominance for cognitive domains
 * WHY:  Models biological brain lateralization (language left, spatial right)
 * HOW:  Dominance weights [0.0-1.0] route processing to appropriate hemisphere
 *
 * BIOLOGICAL BASIS:
 * - Language: 95% left hemisphere (Broca's/Wernicke's areas)
 * - Spatial: 80% right hemisphere (parietal cortex)
 * - Fine motor: Contralateral (left brain = right hand)
 * - Face recognition: 85% right hemisphere (fusiform face area)
 * - Emotion processing: Right hemisphere dominant for recognition
 *
 * PLASTICITY:
 * - Dominance can shift with training/injury (neuroplasticity)
 * - Shift rate configurable for learning scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_LATERALIZATION_H
#define NIMCP_LATERALIZATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Hemisphere identifier
 */
typedef enum {
    HEMISPHERE_LEFT = 0,
    HEMISPHERE_RIGHT = 1,
    HEMISPHERE_COUNT
} hemisphere_id_t;

/**
 * @brief Cognitive domains with lateralized processing
 *
 * BIOLOGICAL BASIS:
 * - Language: Broca's area (production), Wernicke's (comprehension) - LEFT
 * - Spatial: Parietal cortex - RIGHT
 * - Motor Fine: Contralateral primary motor cortex
 * - Emotion: Amygdala/insula processing - RIGHT dominant
 * - Face: Fusiform face area - RIGHT
 */
typedef enum {
    COGNITIVE_DOMAIN_LANGUAGE,           // Speech, grammar, semantics
    COGNITIVE_DOMAIN_SPATIAL,            // Navigation, mental rotation
    COGNITIVE_DOMAIN_MOTOR_FINE,         // Precision movements
    COGNITIVE_DOMAIN_MOTOR_GROSS,        // Bilateral coordination
    COGNITIVE_DOMAIN_EMOTION,            // Emotion recognition/processing
    COGNITIVE_DOMAIN_ATTENTION_GLOBAL,   // Forest (whole scene)
    COGNITIVE_DOMAIN_ATTENTION_LOCAL,    // Trees (details)
    COGNITIVE_DOMAIN_MUSIC_MELODY,       // Pitch, contour
    COGNITIVE_DOMAIN_MUSIC_RHYTHM,       // Timing, beat
    COGNITIVE_DOMAIN_FACE_RECOGNITION,   // Face processing
    COGNITIVE_DOMAIN_LOGICAL_REASONING,  // Deduction, analysis
    COGNITIVE_DOMAIN_CREATIVE_THINKING,  // Divergent thinking
    COGNITIVE_DOMAIN_COUNT
} cognitive_domain_t;

/**
 * @brief Handedness affecting motor lateralization
 */
typedef enum {
    HANDEDNESS_RIGHT,        // ~90% of population
    HANDEDNESS_LEFT,         // ~10% of population
    HANDEDNESS_AMBIDEXTROUS  // Mixed dominance
} handedness_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Lateralization profile for hemisphere specialization
 *
 * WHAT: Defines dominance weights for each cognitive domain
 * WHY:  Routes processing to appropriate hemisphere
 * HOW:  Weight 0.0 = right dominant, 1.0 = left dominant, 0.5 = bilateral
 *
 * EXAMPLE VALUES (typical right-handed):
 * - language: 0.95 (strongly left)
 * - spatial: 0.20 (right dominant)
 * - face_recognition: 0.15 (strongly right)
 */
typedef struct {
    // Domain weights [0.0 = right dominant, 1.0 = left dominant]
    float language_dominance;            // 0.95 default
    float spatial_dominance;             // 0.20 default
    float motor_fine_dominance;          // 0.90 default (right hand = left brain)
    float motor_gross_dominance;         // 0.50 default (bilateral)
    float emotion_processing_dominance;  // 0.30 default (right dominant)
    float attention_global_dominance;    // 0.25 default (right - forest)
    float attention_local_dominance;     // 0.75 default (left - trees)
    float music_melody_dominance;        // 0.20 default (right)
    float music_rhythm_dominance;        // 0.80 default (left)
    float face_recognition_dominance;    // 0.15 default (strongly right)
    float logical_reasoning_dominance;   // 0.85 default (left)
    float creative_thinking_dominance;   // 0.35 default (right-leaning)

    // Handedness (affects motor mapping)
    handedness_t handedness;

    // Plasticity configuration
    bool enable_plasticity;              // Allow dominance shifts
    float plasticity_rate;               // Shift rate per update (0.001 default)
    float min_dominance;                 // Minimum dominance (0.05 default)
    float max_dominance;                 // Maximum dominance (0.95 default)
} lateralization_profile_t;

/**
 * @brief Statistics for lateralization tracking
 */
typedef struct {
    uint64_t left_activations[COGNITIVE_DOMAIN_COUNT];
    uint64_t right_activations[COGNITIVE_DOMAIN_COUNT];
    float cumulative_shift[COGNITIVE_DOMAIN_COUNT];
    uint64_t plasticity_events;
} lateralization_stats_t;

//=============================================================================
// Default Profiles
//=============================================================================

/**
 * @brief Get default lateralization profile (right-handed)
 *
 * WHAT: Standard lateralization for typical right-handed brain
 * WHY:  Provides biologically accurate defaults
 * HOW:  Based on neuroimaging studies of hemisphere specialization
 *
 * @return Default profile with typical dominance values
 */
lateralization_profile_t lateralization_default_profile(void);

/**
 * @brief Get left-handed lateralization profile
 *
 * WHAT: Adjusted lateralization for left-handed individuals
 * WHY:  Left-handers show more bilateral and variable lateralization
 * HOW:  Motor reversed, language slightly more bilateral
 *
 * @return Left-handed profile
 */
lateralization_profile_t lateralization_left_handed_profile(void);

/**
 * @brief Get bilateral (ambidextrous) profile
 *
 * WHAT: Symmetric lateralization across hemispheres
 * WHY:  For experiments requiring balanced processing
 * HOW:  All domains at 0.5 dominance
 *
 * @return Bilateral profile
 */
lateralization_profile_t lateralization_bilateral_profile(void);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get dominance value for a cognitive domain
 *
 * @param profile Lateralization profile
 * @param domain Cognitive domain to query
 * @return Dominance value [0.0-1.0], or 0.5 on error
 */
float lateralization_get_dominance(
    const lateralization_profile_t* profile,
    cognitive_domain_t domain
);

/**
 * @brief Get dominant hemisphere for a domain
 *
 * @param profile Lateralization profile
 * @param domain Cognitive domain
 * @return HEMISPHERE_LEFT if dominance > 0.5, else HEMISPHERE_RIGHT
 */
hemisphere_id_t lateralization_get_dominant_hemisphere(
    const lateralization_profile_t* profile,
    cognitive_domain_t domain
);

/**
 * @brief Check if domain is strongly lateralized
 *
 * WHAT: Determine if domain shows clear hemisphere preference
 * WHY:  Strong lateralization (>0.7 or <0.3) suggests specialized routing
 *
 * @param profile Lateralization profile
 * @param domain Cognitive domain
 * @return true if dominance > 0.7 or < 0.3
 */
bool lateralization_is_strongly_lateralized(
    const lateralization_profile_t* profile,
    cognitive_domain_t domain
);

//=============================================================================
// Plasticity Functions
//=============================================================================

/**
 * @brief Shift dominance for a domain (plasticity)
 *
 * WHAT: Adjust hemisphere dominance based on usage/training
 * WHY:  Models neuroplasticity and recovery from injury
 * HOW:  Shift clamped to [min_dominance, max_dominance]
 *
 * @param profile Lateralization profile
 * @param domain Domain to modify
 * @param shift Shift amount (+ve = more left, -ve = more right)
 * @return 0 on success, -1 if plasticity disabled
 */
int lateralization_shift_dominance(
    lateralization_profile_t* profile,
    cognitive_domain_t domain,
    float shift
);

/**
 * @brief Apply usage-based plasticity
 *
 * WHAT: Strengthen hemisphere that was just activated
 * WHY:  "Neurons that fire together wire together"
 * HOW:  Small shift toward activated hemisphere
 *
 * @param profile Lateralization profile
 * @param domain Domain that was activated
 * @param hemisphere Hemisphere that processed
 * @return 0 on success
 */
int lateralization_apply_usage_plasticity(
    lateralization_profile_t* profile,
    cognitive_domain_t domain,
    hemisphere_id_t hemisphere
);

/**
 * @brief Reset lateralization to default
 *
 * @param profile Profile to reset
 * @param handedness Handedness for default values
 */
void lateralization_reset(
    lateralization_profile_t* profile,
    handedness_t handedness
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get string name for cognitive domain
 */
const char* cognitive_domain_name(cognitive_domain_t domain);

/**
 * @brief Get string name for hemisphere
 */
const char* hemisphere_name(hemisphere_id_t hemisphere);

/**
 * @brief Get string name for handedness
 */
const char* handedness_name(handedness_t handedness);

/**
 * @brief Validate lateralization profile
 *
 * @param profile Profile to validate
 * @return true if all values in valid ranges
 */
bool lateralization_validate(const lateralization_profile_t* profile);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_LATERALIZATION_H
