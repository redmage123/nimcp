/**
 * @file nimcp_personality.c
 * @brief Personality, Gender Identity, and Sexual Identity Implementation
 *
 * WHAT: Personality trait generation and behavioral modulation
 * WHY:  Each brain needs unique personality for individuality
 * HOW:  Big Five model + random generation + behavioral mapping
 *
 * @author NIMCP Development Team
 * @date 2025-11-12
 * @version 2.8.0 Phase 12 Enhancement
 */

#include "cognitive/nimcp_personality.h"
#include "utils/platform/nimcp_platform.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
// Internal Constants
//=============================================================================

#define PI 3.14159265358979323846

//=============================================================================
// Helper Functions (All < 50 lines, early returns, WHAT-WHY-HOW docs)
//=============================================================================

/**
 * @brief Initialize random number generator
 *
 * WHAT: Set up RNG with seed
 * WHY:  Reproducible randomness for testing
 * HOW:  Use seed if non-zero, otherwise use time
 *
 * @param seed Seed value (0 = use time)
 *
 * COMPLEXITY: O(1)
 */
static void init_rng(uint32_t seed) {
    if (seed == 0) {
        seed = (uint32_t)time(NULL);
    }
    srand(seed);
}

/**
 * @brief Generate random float in [0, 1]
 *
 * WHAT: Uniform random number generation
 * WHY:  Need random values for trait generation
 * HOW:  Scale rand() output
 *
 * @return Random float in [0, 1]
 *
 * COMPLEXITY: O(1)
 */
static float random_uniform(void) {
    return (float)rand() / (float)RAND_MAX;
}

/**
 * @brief Generate Gaussian random value (Box-Muller transform)
 *
 * WHAT: Normal distribution random number
 * WHY:  Personality traits follow normal distribution
 * HOW:  Box-Muller transform of uniform random
 *
 * @param mean Mean of distribution
 * @param stddev Standard deviation
 * @return Random value from N(mean, stddev²)
 *
 * COMPLEXITY: O(1)
 */
static float random_gaussian(float mean, float stddev) {
    // Box-Muller transform
    float u1 = random_uniform();
    float u2 = random_uniform();

    // Avoid log(0)
    u1 = (u1 < 1e-10f) ? 1e-10f : u1;

    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)PI * u2);
    return mean + stddev * z;
}

/**
 * @brief Clamp float to [0, 1]
 *
 * WHAT: Constrain value to unit interval
 * WHY:  Trait values must be in [0, 1]
 * HOW:  Min/max clamping
 *
 * @param value Input value
 * @return Clamped value in [0, 1]
 *
 * COMPLEXITY: O(1)
 */
static float clamp_01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

/**
 * @brief Sample gender identity randomly
 *
 * WHAT: Random gender based on probabilities
 * WHY:  Configurable gender distribution
 * HOW:  Weighted random selection
 *
 * @param config Generation configuration
 * @return Random gender identity
 *
 * COMPLEXITY: O(1)
 */
static gender_identity_t sample_gender(
    const personality_generation_config_t* config)
{
    float r = random_uniform();
    float female_prob = config ? config->female_probability : 1.0f;
    float male_prob = config ? config->male_probability : 0.0f;
    float nb_prob = config ? config->non_binary_probability : 0.0f;

    // Normalize probabilities
    float total = female_prob + male_prob + nb_prob;
    if (total < 1e-6f) {
        return GENDER_FEMALE; // Default fallback
    }

    female_prob /= total;
    male_prob /= total;

    if (r < female_prob) {
        return GENDER_FEMALE;
    } else if (r < female_prob + male_prob) {
        return GENDER_MALE;
    } else {
        return GENDER_NON_BINARY;
    }
}

/**
 * @brief Sample sexual orientation randomly
 *
 * WHAT: Random sexuality with realistic distribution
 * WHY:  Diverse sexual orientations
 * HOW:  Weighted sampling based on population estimates
 *
 * @return Random sexual orientation
 *
 * DISTRIBUTION (rough population estimates):
 * - Heterosexual: 85%
 * - Homosexual: 5%
 * - Bisexual: 7%
 * - Pansexual: 2%
 * - Asexual: 1%
 *
 * COMPLEXITY: O(1)
 */
static sexual_orientation_t sample_sexuality(void) {
    float r = random_uniform();

    if (r < 0.85f) return SEXUALITY_HETEROSEXUAL;
    if (r < 0.90f) return SEXUALITY_HOMOSEXUAL;
    if (r < 0.97f) return SEXUALITY_BISEXUAL;
    if (r < 0.99f) return SEXUALITY_PANSEXUAL;
    return SEXUALITY_ASEXUAL;
}

//=============================================================================
// Public API Implementation
//=============================================================================

personality_generation_config_t personality_default_generation_config(void) {
    /**
     * WHAT: Return sensible default configuration
     * WHY:  Quick setup for common use cases
     * HOW:  Preset values based on user request and psychology
     */

    personality_generation_config_t config = {
        .trait_mean = 0.5f,
        .trait_stddev = 0.15f,
        .female_probability = 1.0f,  // Default: female (per user request)
        .male_probability = 0.0f,
        .non_binary_probability = 0.0f,
        .seed = 0,  // Time-based
        .enforce_balanced_traits = false
    };

    return config;
}

personality_profile_t personality_generate_random(
    const personality_generation_config_t* config)
{
    /**
     * WHAT: Generate unique random personality
     * WHY:  Each brain should be an individual
     * HOW:  Sample traits from normal distribution, assign identity
     */

    // Initialize RNG
    uint32_t seed = config ? config->seed : 0;
    init_rng(seed);

    // Generate personality traits
    float mean = config ? config->trait_mean : 0.5f;
    float stddev = config ? config->trait_stddev : 0.15f;

    personality_traits_t traits;
    traits.openness = clamp_01(random_gaussian(mean, stddev));
    traits.conscientiousness = clamp_01(random_gaussian(mean, stddev));
    traits.extraversion = clamp_01(random_gaussian(mean, stddev));
    traits.agreeableness = clamp_01(random_gaussian(mean, stddev));
    traits.neuroticism = clamp_01(random_gaussian(mean, stddev));

    // Generate identity
    identity_profile_t identity;
    memset(&identity, 0, sizeof(identity_profile_t));

    identity.gender = sample_gender(config);
    identity.sexuality = sample_sexuality();

    identity.gender_certainty = clamp_01(random_gaussian(0.9f, 0.1f));
    identity.sexuality_certainty = clamp_01(random_gaussian(0.8f, 0.15f));

    identity.gender_is_core_identity = (random_uniform() > 0.5f);
    identity.sexuality_is_core_identity = (random_uniform() > 0.3f);

    // Create profile
    personality_profile_t profile;
    profile.traits = traits;
    profile.identity = identity;
    profile.created_timestamp_ms = nimcp_platform_time_monotonic_ms();
    profile.seed = seed;
    profile.was_randomly_generated = true;

    // Compute behavioral modifiers
    personality_compute_modifiers(&profile);

    return profile;
}

personality_profile_t personality_create_custom(
    const personality_traits_t* traits,
    const identity_profile_t* identity)
{
    /**
     * WHAT: Create personality from explicit specification
     * WHY:  Allow precise control for testing
     * HOW:  Direct assignment with validation
     */

    // Guard: NULL checks
    if (!traits || !identity) {
        // Return default personality on error
        return personality_generate_random(NULL);
    }

    personality_profile_t profile;
    memset(&profile, 0, sizeof(personality_profile_t));

    // Copy and validate traits
    profile.traits.openness = clamp_01(traits->openness);
    profile.traits.conscientiousness = clamp_01(traits->conscientiousness);
    profile.traits.extraversion = clamp_01(traits->extraversion);
    profile.traits.agreeableness = clamp_01(traits->agreeableness);
    profile.traits.neuroticism = clamp_01(traits->neuroticism);

    // Copy identity
    memcpy(&profile.identity, identity, sizeof(identity_profile_t));

    // Clamp certainties
    profile.identity.gender_certainty = clamp_01(identity->gender_certainty);
    profile.identity.sexuality_certainty = clamp_01(identity->sexuality_certainty);

    profile.created_timestamp_ms = nimcp_platform_time_monotonic_ms();
    profile.seed = 0;
    profile.was_randomly_generated = false;

    // Compute behavioral modifiers
    personality_compute_modifiers(&profile);

    return profile;
}

void personality_compute_modifiers(personality_profile_t* profile) {
    /**
     * WHAT: Calculate behavioral modifiers from traits
     * WHY:  Traits must influence brain behavior
     * HOW:  Linear mapping: [0,1] → [-0.5, +0.5]
     *
     * FORMULA: modifier = trait - 0.5
     */

    // Guard: NULL check
    if (!profile) return;

    // Map each trait to [-0.5, +0.5]
    profile->curiosity_modifier = profile->traits.openness - 0.5f;
    profile->planning_modifier = profile->traits.conscientiousness - 0.5f;
    profile->social_drive_modifier = profile->traits.extraversion - 0.5f;
    profile->empathy_modifier = profile->traits.agreeableness - 0.5f;
    profile->stress_sensitivity_modifier = profile->traits.neuroticism - 0.5f;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* personality_get_gender_string(const personality_profile_t* profile) {
    /**
     * WHAT: Get human-readable gender string
     * WHY:  User-facing text generation
     * HOW:  Map enum to string
     */

    // Guard: NULL check
    if (!profile) return "Unknown";

    switch (profile->identity.gender) {
        case GENDER_MALE:       return "Male";
        case GENDER_FEMALE:     return "Female";
        case GENDER_NON_BINARY: return "Non-Binary";
        case GENDER_GENDERFLUID: return "Genderfluid";
        case GENDER_AGENDER:    return "Agender";
        case GENDER_CUSTOM:
            if (profile->identity.gender_custom_label[0] != '\0') {
                return profile->identity.gender_custom_label;
            }
            return "Custom";
        default: return "Unknown";
    }
}

const char* personality_get_sexuality_string(const personality_profile_t* profile) {
    /**
     * WHAT: Get human-readable sexuality string
     * WHY:  User-facing text generation
     * HOW:  Map enum to string
     */

    // Guard: NULL check
    if (!profile) return "Unknown";

    switch (profile->identity.sexuality) {
        case SEXUALITY_HETEROSEXUAL: return "Heterosexual";
        case SEXUALITY_HOMOSEXUAL:   return "Homosexual";
        case SEXUALITY_BISEXUAL:     return "Bisexual";
        case SEXUALITY_PANSEXUAL:    return "Pansexual";
        case SEXUALITY_ASEXUAL:      return "Asexual";
        case SEXUALITY_DEMISEXUAL:   return "Demisexual";
        case SEXUALITY_CUSTOM:
            if (profile->identity.sexuality_custom_label[0] != '\0') {
                return profile->identity.sexuality_custom_label;
            }
            return "Custom";
        default: return "Unknown";
    }
}

bool personality_get_pronouns(
    const personality_profile_t* profile,
    char* subject, size_t subject_len,
    char* object, size_t object_len,
    char* possessive, size_t possessive_len)
{
    /**
     * WHAT: Get pronouns for gender identity
     * WHY:  Respectful language generation
     * HOW:  Map gender to pronoun sets
     */

    // Guard: NULL checks
    if (!profile || !subject || !object || !possessive) return false;
    if (subject_len < 8 || object_len < 8 || possessive_len < 8) return false;

    // Determine pronouns based on gender
    const char* subj = "they";
    const char* obj = "them";
    const char* poss = "their";

    switch (profile->identity.gender) {
        case GENDER_FEMALE:
            subj = "she";
            obj = "her";
            poss = "her";
            break;

        case GENDER_MALE:
            subj = "he";
            obj = "him";
            poss = "his";
            break;

        case GENDER_NON_BINARY:
        case GENDER_GENDERFLUID:
        case GENDER_AGENDER:
        case GENDER_CUSTOM:
            // Default to they/them/their for non-binary identities
            subj = "they";
            obj = "them";
            poss = "their";
            break;

        default:
            subj = "they";
            obj = "them";
            poss = "their";
            break;
    }

    // Copy to output buffers
    strncpy(subject, subj, subject_len - 1);
    subject[subject_len - 1] = '\0';

    strncpy(object, obj, object_len - 1);
    object[object_len - 1] = '\0';

    strncpy(possessive, poss, possessive_len - 1);
    possessive[possessive_len - 1] = '\0';

    return true;
}

bool personality_generate_summary(
    const personality_profile_t* profile,
    char* buffer,
    size_t buffer_size)
{
    /**
     * WHAT: Generate human-readable summary
     * WHY:  Debugging and user interface
     * HOW:  Format all traits and identity into text
     */

    // Guard: NULL checks
    if (!profile || !buffer || buffer_size == 0) return false;

    // Trait descriptors
    const char* openness_desc = (profile->traits.openness > 0.7f) ? "Creative" :
                                (profile->traits.openness < 0.3f) ? "Conventional" :
                                "Moderate-Openness";

    const char* conscient_desc = (profile->traits.conscientiousness > 0.7f) ? "Organized" :
                                 (profile->traits.conscientiousness < 0.3f) ? "Spontaneous" :
                                 "Moderate-Conscientiousness";

    const char* extraver_desc = (profile->traits.extraversion > 0.7f) ? "Extraverted" :
                                (profile->traits.extraversion < 0.3f) ? "Introverted" :
                                "Ambivert";

    const char* agreeable_desc = (profile->traits.agreeableness > 0.7f) ? "Compassionate" :
                                 (profile->traits.agreeableness < 0.3f) ? "Direct" :
                                 "Moderate-Agreeableness";

    const char* neurotic_desc = (profile->traits.neuroticism > 0.7f) ? "Sensitive" :
                                (profile->traits.neuroticism < 0.3f) ? "Emotionally-Stable" :
                                "Moderate-Stability";

    int written = snprintf(buffer, buffer_size,
        "Personality: %s (%.2f), %s (%.2f), %s (%.2f), %s (%.2f), %s (%.2f)\n"
        "Gender: %s | Sexuality: %s",
        openness_desc, profile->traits.openness,
        conscient_desc, profile->traits.conscientiousness,
        extraver_desc, profile->traits.extraversion,
        agreeable_desc, profile->traits.agreeableness,
        neurotic_desc, profile->traits.neuroticism,
        personality_get_gender_string(profile),
        personality_get_sexuality_string(profile));

    return (written > 0 && (size_t)written < buffer_size);
}
