/**
 * @file nimcp_personality.h
 * @brief Personality, Gender Identity, and Sexual Identity System
 *
 * WHAT: Individual personality traits, gender identity, and sexual orientation
 * WHY:  Make each brain unique with distinct behavioral tendencies
 * HOW:  Big Five personality model + gender/sexuality identity structures
 *
 * PURPOSE:
 * Humans exhibit unique personalities that influence behavior, decision-making,
 * emotional responses, and social interactions. This module provides:
 * - Big Five personality traits (OCEAN model)
 * - Gender identity representation
 * - Sexual orientation representation
 * - Personality-driven behavior modulation
 *
 * DESIGN PRINCIPLES:
 * 1. Scientific Validity: Use Big Five (most validated personality model)
 * 2. Inclusivity: Comprehensive gender and sexuality representation
 * 3. Behavioral Impact: Personality traits modulate cognitive processes
 * 4. Uniqueness: Random generation creates diverse individuals
 * 5. Configurability: Allow explicit personality specification
 *
 * BIOLOGICAL INSPIRATION:
 * - Personality correlates with brain structure (fMRI studies)
 * - Traits influence neurotransmitter levels (serotonin, dopamine)
 * - Gender/sexuality involve hypothalamus, amygdala, and hormonal systems
 * - Personality is ~40-60% heritable, partially malleable with experience
 *
 * BIG FIVE MODEL (OCEAN):
 * - Openness: Creativity, curiosity, appreciation for novelty
 * - Conscientiousness: Organization, dependability, self-discipline
 * - Extraversion: Sociability, assertiveness, energy level
 * - Agreeableness: Compassion, cooperation, trust
 * - Neuroticism: Emotional instability, anxiety, moodiness
 *
 * BEHAVIORAL EFFECTS:
 * - High Openness: More exploratory learning, creative problem-solving
 * - High Conscientiousness: Better planning, task persistence
 * - High Extraversion: More social interactions, higher arousal baseline
 * - High Agreeableness: More cooperative, empathetic responses
 * - High Neuroticism: Stronger negative emotions, higher stress sensitivity
 *
 * @author NIMCP Development Team
 * @date 2025-11-12
 * @version 2.8.0 Phase 12 Enhancement
 */

#ifndef NIMCP_PERSONALITY_H
#define NIMCP_PERSONALITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define PERSONALITY_MAX_CUSTOM_LABEL_LEN 64

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Gender identity categories
 *
 * WHAT: Comprehensive gender identity representation
 * WHY:  Respect diversity of gender experience
 * HOW:  Inclusive categories with custom option
 */
typedef enum {
    GENDER_MALE,           /**< Male gender identity */
    GENDER_FEMALE,         /**< Female gender identity (DEFAULT) */
    GENDER_NON_BINARY,     /**< Non-binary gender identity */
    GENDER_GENDERFLUID,    /**< Fluid gender identity */
    GENDER_AGENDER,        /**< No gender identity */
    GENDER_CUSTOM          /**< Custom/other (see custom_label) */
} gender_identity_t;

/**
 * @brief Sexual orientation categories
 *
 * WHAT: Comprehensive sexual orientation representation
 * WHY:  Respect diversity of sexual/romantic attraction
 * HOW:  Inclusive categories with custom option
 */
typedef enum {
    SEXUALITY_HETEROSEXUAL, /**< Attraction to different gender */
    SEXUALITY_HOMOSEXUAL,   /**< Attraction to same gender */
    SEXUALITY_BISEXUAL,     /**< Attraction to multiple genders */
    SEXUALITY_PANSEXUAL,    /**< Attraction regardless of gender */
    SEXUALITY_ASEXUAL,      /**< Little/no sexual attraction */
    SEXUALITY_DEMISEXUAL,   /**< Attraction only with emotional bond */
    SEXUALITY_CUSTOM        /**< Custom/other (see custom_label) */
} sexual_orientation_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Big Five personality traits (OCEAN model)
 *
 * WHAT: Five-factor model of personality
 * WHY:  Most scientifically validated personality framework
 * HOW:  Each trait is a continuous value [0, 1]
 *
 * INTERPRETATION:
 * - 0.0-0.3: Low on trait
 * - 0.3-0.7: Moderate on trait
 * - 0.7-1.0: High on trait
 *
 * TYPICAL DISTRIBUTION: Normal distribution centered at 0.5, σ ≈ 0.15
 */
typedef struct {
    /**
     * Openness to Experience [0-1]
     *
     * HIGH: Creative, curious, imaginative, appreciates novelty
     * LOW:  Conventional, pragmatic, prefers routine
     *
     * BEHAVIORAL EFFECTS:
     * - Modulates curiosity drive
     * - Influences exploratory learning
     * - Affects response to novel stimuli
     */
    float openness;

    /**
     * Conscientiousness [0-1]
     *
     * HIGH: Organized, disciplined, dependable, plans ahead
     * LOW:  Spontaneous, flexible, less organized
     *
     * BEHAVIORAL EFFECTS:
     * - Modulates executive control strength
     * - Influences goal persistence
     * - Affects planning depth
     */
    float conscientiousness;

    /**
     * Extraversion [0-1]
     *
     * HIGH: Outgoing, energetic, sociable, assertive
     * LOW:  Reserved, reflective, independent (introversion)
     *
     * BEHAVIORAL EFFECTS:
     * - Modulates social drive
     * - Influences arousal baseline
     * - Affects reward sensitivity
     */
    float extraversion;

    /**
     * Agreeableness [0-1]
     *
     * HIGH: Compassionate, cooperative, trusting, empathetic
     * LOW:  Skeptical, competitive, direct
     *
     * BEHAVIORAL EFFECTS:
     * - Modulates empathetic responses
     * - Influences cooperation likelihood
     * - Affects conflict resolution style
     */
    float agreeableness;

    /**
     * Neuroticism [0-1]
     *
     * HIGH: Emotionally reactive, anxious, prone to stress
     * LOW:  Calm, emotionally stable, resilient (emotional stability)
     *
     * BEHAVIORAL EFFECTS:
     * - Modulates stress sensitivity
     * - Influences negative emotion intensity
     * - Affects baseline anxiety level
     */
    float neuroticism;
} personality_traits_t;

/**
 * @brief Gender and sexual identity
 *
 * WHAT: Complete gender and sexuality representation
 * WHY:  Identity is fundamental to self-concept
 * HOW:  Categorical identity + custom labels + certainty
 */
typedef struct {
    gender_identity_t gender;               /**< Gender identity */
    sexual_orientation_t sexuality;         /**< Sexual orientation */

    char gender_custom_label[PERSONALITY_MAX_CUSTOM_LABEL_LEN];    /**< Custom gender label if GENDER_CUSTOM */
    char sexuality_custom_label[PERSONALITY_MAX_CUSTOM_LABEL_LEN]; /**< Custom sexuality label if SEXUALITY_CUSTOM */

    float gender_certainty;                 /**< [0-1] Certainty about gender identity */
    float sexuality_certainty;              /**< [0-1] Certainty about sexual orientation */

    bool gender_is_core_identity;           /**< Is gender central to self-concept? */
    bool sexuality_is_core_identity;        /**< Is sexuality central to self-concept? */
} identity_profile_t;

/**
 * @brief Complete personality profile
 *
 * WHAT: Combined personality traits and identity
 * WHY:  Unique individual profile for each brain
 * HOW:  Traits + identity + metadata
 */
typedef struct {
    personality_traits_t traits;            /**< Big Five personality traits */
    identity_profile_t identity;            /**< Gender and sexual identity */

    uint64_t created_timestamp_ms;          /**< When was this personality created? */
    uint32_t seed;                          /**< RNG seed for reproducibility */
    bool was_randomly_generated;            /**< true if random, false if configured */

    // Behavioral modifiers (computed from traits)
    float curiosity_modifier;               /**< [-0.5, +0.5] Openness-based curiosity boost */
    float planning_modifier;                /**< [-0.5, +0.5] Conscientiousness-based planning boost */
    float social_drive_modifier;            /**< [-0.5, +0.5] Extraversion-based social interaction boost */
    float empathy_modifier;                 /**< [-0.5, +0.5] Agreeableness-based empathy boost */
    float stress_sensitivity_modifier;      /**< [-0.5, +0.5] Neuroticism-based stress sensitivity */
} personality_profile_t;

/**
 * @brief Personality generation configuration
 *
 * WHAT: Options for personality generation
 * WHY:  Allow control over random generation
 * HOW:  Specify means, variances, constraints
 */
typedef struct {
    // Trait distribution parameters (for random generation)
    float trait_mean;                       /**< Mean for all traits (default: 0.5) */
    float trait_stddev;                     /**< Standard deviation (default: 0.15) */

    // Identity preferences (for random generation)
    float female_probability;               /**< P(female) when random (default: 0.5) */
    float male_probability;                 /**< P(male) when random (default: 0.5) */
    float non_binary_probability;           /**< P(non-binary) when random (default: 0.0) */

    uint32_t seed;                          /**< RNG seed (0 = time-based) */
    bool enforce_balanced_traits;           /**< Ensure no extreme trait combinations */
} personality_generation_config_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Generate random personality profile
 *
 * WHAT: Create unique personality with random traits
 * WHY:  Each brain instance should be unique
 * HOW:  Sample from normal distributions, assign identity
 *
 * @param config Generation parameters (NULL = default)
 * @return Personality profile
 *
 * EXAMPLE:
 * ```c
 * personality_profile_t personality = personality_generate_random(NULL);
 * printf("Openness: %.2f, Gender: %s\n",
 *        personality.traits.openness,
 *        personality_get_gender_string(&personality));
 * ```
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (if config != NULL and seed != 0)
 */
personality_profile_t personality_generate_random(
    const personality_generation_config_t* config);

/**
 * @brief Create personality with specific traits
 *
 * WHAT: Manually specify all personality parameters
 * WHY:  Allow precise control for testing or specific needs
 * HOW:  Direct assignment with validation
 *
 * @param traits Big Five trait values
 * @param identity Gender and sexuality identity
 * @return Personality profile
 *
 * VALIDATION:
 * - All trait values clamped to [0, 1]
 * - Certainty values clamped to [0, 1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
personality_profile_t personality_create_custom(
    const personality_traits_t* traits,
    const identity_profile_t* identity);

/**
 * @brief Get default personality generation config
 *
 * WHAT: Sensible defaults for personality generation
 * WHY:  Quick setup for common use cases
 * HOW:  Preset values based on psychological research
 *
 * DEFAULTS:
 * - trait_mean = 0.5
 * - trait_stddev = 0.15 (matches population distribution)
 * - female_probability = 1.0 (per user request)
 * - seed = 0 (time-based)
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
personality_generation_config_t personality_default_generation_config(void);

/**
 * @brief Compute behavioral modifiers from personality traits
 *
 * WHAT: Calculate how traits affect brain behavior
 * WHY:  Personality must influence cognitive processes
 * HOW:  Linear mapping: trait ∈ [0,1] → modifier ∈ [-0.5, +0.5]
 *
 * @param profile [IN/OUT] Personality profile (modifiers updated)
 *
 * FORMULA: modifier = (trait - 0.5)
 * - Low trait (0.0): modifier = -0.5 (reduce behavior)
 * - Mid trait (0.5): modifier = 0.0 (neutral)
 * - High trait (1.0): modifier = +0.5 (enhance behavior)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void personality_compute_modifiers(personality_profile_t* profile);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get string representation of gender identity
 *
 * @param profile Personality profile
 * @return String representation (e.g., "Female", "Non-Binary")
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
const char* personality_get_gender_string(const personality_profile_t* profile);

/**
 * @brief Get string representation of sexual orientation
 *
 * @param profile Personality profile
 * @return String representation (e.g., "Heterosexual", "Pansexual")
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
const char* personality_get_sexuality_string(const personality_profile_t* profile);

/**
 * @brief Get pronouns based on gender identity
 *
 * WHAT: Appropriate pronouns for identity
 * WHY:  Respectful language generation
 * HOW:  Map gender to pronouns (subject/object/possessive)
 *
 * @param profile Personality profile
 * @param subject [OUT] Subject pronoun (e.g., "she", "he", "they")
 * @param object [OUT] Object pronoun (e.g., "her", "him", "them")
 * @param possessive [OUT] Possessive (e.g., "her", "his", "their")
 * @param subject_len Buffer size for subject
 * @param object_len Buffer size for object
 * @param possessive_len Buffer size for possessive
 * @return true on success
 *
 * EXAMPLES:
 * - GENDER_FEMALE: she/her/her
 * - GENDER_MALE: he/him/his
 * - GENDER_NON_BINARY: they/them/their
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool personality_get_pronouns(
    const personality_profile_t* profile,
    char* subject, size_t subject_len,
    char* object, size_t object_len,
    char* possessive, size_t possessive_len);

/**
 * @brief Generate personality summary string
 *
 * WHAT: Human-readable personality description
 * WHY:  Debugging and user-facing descriptions
 * HOW:  Format traits and identity into text
 *
 * @param profile Personality profile
 * @param buffer [OUT] Output buffer
 * @param buffer_size Buffer size
 * @return true on success
 *
 * GENERATES:
 * "Personality: Open (0.72), Conscientious (0.45), Extraverted (0.81),
 *  Agreeable (0.65), Emotionally Stable (0.58)
 *  Gender: Female | Sexuality: Bisexual"
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool personality_generate_summary(
    const personality_profile_t* profile,
    char* buffer,
    size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PERSONALITY_H
