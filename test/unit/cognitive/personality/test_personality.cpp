/**
 * @file test_personality.cpp
 * @brief Unit tests for personality, gender, and sexual identity system
 *
 * WHAT: Comprehensive tests for personality generation and behavior
 * WHY:  Ensure personality system works correctly before integration
 * HOW:  GTest framework with edge cases and statistical validation
 *
 * TEST COVERAGE:
 * - Personality trait generation (random and custom)
 * - Gender identity handling
 * - Sexual orientation handling
 * - Behavioral modifier computation
 * - Utility functions (string conversion, pronouns)
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-11-12
 */

#include <gtest/gtest.h>

    #include "cognitive/nimcp_personality.h"

#include <cmath>
#include <string>
#include <map>

//=============================================================================
// Test Fixture
//=============================================================================

class PersonalityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed
    }

    void TearDown() override {
        // Cleanup if needed
    }

    // Helper: Check if value is in valid range [0, 1]
    bool is_valid_trait(float value) {
        return value >= 0.0f && value <= 1.0f;
    }

    // Helper: Check if modifier is in valid range [-0.5, +0.5]
    bool is_valid_modifier(float modifier) {
        return modifier >= -0.5f && modifier <= 0.5f;
    }
};

//=============================================================================
// Test 1: Default Generation Config
//=============================================================================

TEST_F(PersonalityTest, DefaultGenerationConfig) {
    personality_generation_config_t config = personality_default_generation_config();

    EXPECT_FLOAT_EQ(config.trait_mean, 0.5f);
    EXPECT_FLOAT_EQ(config.trait_stddev, 0.15f);
    EXPECT_FLOAT_EQ(config.female_probability, 1.0f);  // Default: female
    EXPECT_FLOAT_EQ(config.male_probability, 0.0f);
    EXPECT_FLOAT_EQ(config.non_binary_probability, 0.0f);
    EXPECT_EQ(config.seed, 0u);  // Time-based
    EXPECT_FALSE(config.enforce_balanced_traits);
}

//=============================================================================
// Test 2: Random Personality Generation
//=============================================================================

TEST_F(PersonalityTest, RandomPersonalityGeneration) {
    personality_profile_t profile = personality_generate_random(nullptr);

    // Check all traits are in [0, 1]
    EXPECT_TRUE(is_valid_trait(profile.traits.openness));
    EXPECT_TRUE(is_valid_trait(profile.traits.conscientiousness));
    EXPECT_TRUE(is_valid_trait(profile.traits.extraversion));
    EXPECT_TRUE(is_valid_trait(profile.traits.agreeableness));
    EXPECT_TRUE(is_valid_trait(profile.traits.neuroticism));

    // Check behavioral modifiers are in [-0.5, +0.5]
    EXPECT_TRUE(is_valid_modifier(profile.curiosity_modifier));
    EXPECT_TRUE(is_valid_modifier(profile.planning_modifier));
    EXPECT_TRUE(is_valid_modifier(profile.social_drive_modifier));
    EXPECT_TRUE(is_valid_modifier(profile.empathy_modifier));
    EXPECT_TRUE(is_valid_modifier(profile.stress_sensitivity_modifier));

    // Check identity fields
    EXPECT_TRUE(profile.identity.gender_certainty >= 0.0f &&
                profile.identity.gender_certainty <= 1.0f);
    EXPECT_TRUE(profile.identity.sexuality_certainty >= 0.0f &&
                profile.identity.sexuality_certainty <= 1.0f);

    // Check metadata
    EXPECT_TRUE(profile.was_randomly_generated);
    EXPECT_GT(profile.created_timestamp_ms, 0u);
}

//=============================================================================
// Test 3: Reproducible Generation with Seed
//=============================================================================

TEST_F(PersonalityTest, ReproducibleGeneration) {
    personality_generation_config_t config = personality_default_generation_config();
    config.seed = 12345;

    personality_profile_t profile1 = personality_generate_random(&config);
    personality_profile_t profile2 = personality_generate_random(&config);

    // Same seed should produce identical traits
    EXPECT_FLOAT_EQ(profile1.traits.openness, profile2.traits.openness);
    EXPECT_FLOAT_EQ(profile1.traits.conscientiousness, profile2.traits.conscientiousness);
    EXPECT_FLOAT_EQ(profile1.traits.extraversion, profile2.traits.extraversion);
    EXPECT_FLOAT_EQ(profile1.traits.agreeableness, profile2.traits.agreeableness);
    EXPECT_FLOAT_EQ(profile1.traits.neuroticism, profile2.traits.neuroticism);

    // Gender and sexuality should also match
    EXPECT_EQ(profile1.identity.gender, profile2.identity.gender);
    EXPECT_EQ(profile1.identity.sexuality, profile2.identity.sexuality);
}

//=============================================================================
// Test 4: Custom Personality Creation
//=============================================================================

TEST_F(PersonalityTest, CustomPersonalityCreation) {
    personality_traits_t traits;
    traits.openness = 0.8f;
    traits.conscientiousness = 0.6f;
    traits.extraversion = 0.7f;
    traits.agreeableness = 0.9f;
    traits.neuroticism = 0.3f;

    identity_profile_t identity = {};
    identity.gender = GENDER_NON_BINARY;
    identity.sexuality = SEXUALITY_PANSEXUAL;
    identity.gender_certainty = 1.0f;
    identity.sexuality_certainty = 0.9f;
    identity.gender_is_core_identity = true;
    identity.sexuality_is_core_identity = true;

    personality_profile_t profile = personality_create_custom(&traits, &identity);

    // Check traits match
    EXPECT_FLOAT_EQ(profile.traits.openness, 0.8f);
    EXPECT_FLOAT_EQ(profile.traits.conscientiousness, 0.6f);
    EXPECT_FLOAT_EQ(profile.traits.extraversion, 0.7f);
    EXPECT_FLOAT_EQ(profile.traits.agreeableness, 0.9f);
    EXPECT_FLOAT_EQ(profile.traits.neuroticism, 0.3f);

    // Check identity matches
    EXPECT_EQ(profile.identity.gender, GENDER_NON_BINARY);
    EXPECT_EQ(profile.identity.sexuality, SEXUALITY_PANSEXUAL);
    EXPECT_FLOAT_EQ(profile.identity.gender_certainty, 1.0f);
    EXPECT_FLOAT_EQ(profile.identity.sexuality_certainty, 0.9f);

    // Check metadata
    EXPECT_FALSE(profile.was_randomly_generated);
}

//=============================================================================
// Test 5: Trait Clamping (Values Outside [0, 1])
//=============================================================================

TEST_F(PersonalityTest, TraitClamping) {
    personality_traits_t traits;
    traits.openness = 1.5f;  // Too high
    traits.conscientiousness = -0.3f;  // Too low
    traits.extraversion = 0.5f;  // Valid
    traits.agreeableness = 2.0f;  // Too high
    traits.neuroticism = -1.0f;  // Too low

    identity_profile_t identity = {};
    identity.gender = GENDER_FEMALE;
    identity.sexuality = SEXUALITY_HETEROSEXUAL;
    identity.gender_certainty = 0.5f;
    identity.sexuality_certainty = 0.5f;

    personality_profile_t profile = personality_create_custom(&traits, &identity);

    // Check clamping
    EXPECT_FLOAT_EQ(profile.traits.openness, 1.0f);  // Clamped
    EXPECT_FLOAT_EQ(profile.traits.conscientiousness, 0.0f);  // Clamped
    EXPECT_FLOAT_EQ(profile.traits.extraversion, 0.5f);  // Unchanged
    EXPECT_FLOAT_EQ(profile.traits.agreeableness, 1.0f);  // Clamped
    EXPECT_FLOAT_EQ(profile.traits.neuroticism, 0.0f);  // Clamped
}

//=============================================================================
// Test 6: Behavioral Modifier Computation
//=============================================================================

TEST_F(PersonalityTest, BehavioralModifierComputation) {
    personality_traits_t traits;
    traits.openness = 1.0f;  // High → +0.5 modifier
    traits.conscientiousness = 0.0f;  // Low → -0.5 modifier
    traits.extraversion = 0.5f;  // Mid → 0.0 modifier
    traits.agreeableness = 0.75f;  // Above mid → +0.25 modifier
    traits.neuroticism = 0.25f;  // Below mid → -0.25 modifier

    identity_profile_t identity = {};
    identity.gender = GENDER_MALE;
    identity.sexuality = SEXUALITY_BISEXUAL;
    identity.gender_certainty = 1.0f;
    identity.sexuality_certainty = 1.0f;

    personality_profile_t profile = personality_create_custom(&traits, &identity);

    // Check modifiers (formula: modifier = trait - 0.5)
    EXPECT_FLOAT_EQ(profile.curiosity_modifier, 0.5f);
    EXPECT_FLOAT_EQ(profile.planning_modifier, -0.5f);
    EXPECT_FLOAT_EQ(profile.social_drive_modifier, 0.0f);
    EXPECT_FLOAT_EQ(profile.empathy_modifier, 0.25f);
    EXPECT_FLOAT_EQ(profile.stress_sensitivity_modifier, -0.25f);
}

//=============================================================================
// Test 7: Gender Identity Strings
//=============================================================================

TEST_F(PersonalityTest, GenderIdentityStrings) {
    personality_traits_t traits = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    identity_profile_t identity = {};
    identity.gender_certainty = 1.0f;
    identity.sexuality_certainty = 1.0f;

    // Test each gender
    identity.gender = GENDER_MALE;
    identity.sexuality = SEXUALITY_HETEROSEXUAL;
    personality_profile_t profile_male = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_gender_string(&profile_male), "Male");

    identity.gender = GENDER_FEMALE;
    personality_profile_t profile_female = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_gender_string(&profile_female), "Female");

    identity.gender = GENDER_NON_BINARY;
    personality_profile_t profile_nb = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_gender_string(&profile_nb), "Non-Binary");

    identity.gender = GENDER_AGENDER;
    personality_profile_t profile_ag = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_gender_string(&profile_ag), "Agender");
}

//=============================================================================
// Test 8: Sexual Orientation Strings
//=============================================================================

TEST_F(PersonalityTest, SexualOrientationStrings) {
    personality_traits_t traits = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    identity_profile_t identity = {};
    identity.gender = GENDER_FEMALE;
    identity.gender_certainty = 1.0f;
    identity.sexuality_certainty = 1.0f;

    // Test each sexuality
    identity.sexuality = SEXUALITY_HETEROSEXUAL;
    personality_profile_t profile = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_sexuality_string(&profile), "Heterosexual");

    identity.sexuality = SEXUALITY_HOMOSEXUAL;
    profile = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_sexuality_string(&profile), "Homosexual");

    identity.sexuality = SEXUALITY_BISEXUAL;
    profile = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_sexuality_string(&profile), "Bisexual");

    identity.sexuality = SEXUALITY_PANSEXUAL;
    profile = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_sexuality_string(&profile), "Pansexual");

    identity.sexuality = SEXUALITY_ASEXUAL;
    profile = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_sexuality_string(&profile), "Asexual");

    identity.sexuality = SEXUALITY_DEMISEXUAL;
    profile = personality_create_custom(&traits, &identity);
    EXPECT_STREQ(personality_get_sexuality_string(&profile), "Demisexual");
}

//=============================================================================
// Test 9: Pronoun Generation
//=============================================================================

TEST_F(PersonalityTest, PronounGeneration) {
    personality_traits_t traits = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    identity_profile_t identity = {};
    identity.gender_certainty = 1.0f;
    identity.sexuality = SEXUALITY_HETEROSEXUAL;
    identity.sexuality_certainty = 1.0f;

    char subject[16], object[16], possessive[16];

    // Female pronouns
    identity.gender = GENDER_FEMALE;
    personality_profile_t profile_f = personality_create_custom(&traits, &identity);
    EXPECT_TRUE(personality_get_pronouns(&profile_f, subject, sizeof(subject),
                                         object, sizeof(object),
                                         possessive, sizeof(possessive)));
    EXPECT_STREQ(subject, "she");
    EXPECT_STREQ(object, "her");
    EXPECT_STREQ(possessive, "her");

    // Male pronouns
    identity.gender = GENDER_MALE;
    personality_profile_t profile_m = personality_create_custom(&traits, &identity);
    EXPECT_TRUE(personality_get_pronouns(&profile_m, subject, sizeof(subject),
                                         object, sizeof(object),
                                         possessive, sizeof(possessive)));
    EXPECT_STREQ(subject, "he");
    EXPECT_STREQ(object, "him");
    EXPECT_STREQ(possessive, "his");

    // Non-binary pronouns (they/them/their)
    identity.gender = GENDER_NON_BINARY;
    personality_profile_t profile_nb = personality_create_custom(&traits, &identity);
    EXPECT_TRUE(personality_get_pronouns(&profile_nb, subject, sizeof(subject),
                                         object, sizeof(object),
                                         possessive, sizeof(possessive)));
    EXPECT_STREQ(subject, "they");
    EXPECT_STREQ(object, "them");
    EXPECT_STREQ(possessive, "their");
}

//=============================================================================
// Test 10: Summary Generation
//=============================================================================

TEST_F(PersonalityTest, SummaryGeneration) {
    personality_traits_t traits;
    traits.openness = 0.85f;  // Creative
    traits.conscientiousness = 0.45f;  // Moderate
    traits.extraversion = 0.90f;  // Extraverted
    traits.agreeableness = 0.75f;  // Compassionate
    traits.neuroticism = 0.25f;  // Emotionally Stable

    identity_profile_t identity = {};
    identity.gender = GENDER_FEMALE;
    identity.sexuality = SEXUALITY_BISEXUAL;
    identity.gender_certainty = 1.0f;
    identity.sexuality_certainty = 0.8f;

    personality_profile_t profile = personality_create_custom(&traits, &identity);

    char summary[1024];
    EXPECT_TRUE(personality_generate_summary(&profile, summary, sizeof(summary)));

    // Check summary contains key information
    std::string summary_str(summary);
    EXPECT_NE(summary_str.find("Creative"), std::string::npos);
    EXPECT_NE(summary_str.find("Extraverted"), std::string::npos);
    EXPECT_NE(summary_str.find("Female"), std::string::npos);
    EXPECT_NE(summary_str.find("Bisexual"), std::string::npos);
}

//=============================================================================
// Test 11: Default Gender Probability (Female)
//=============================================================================

TEST_F(PersonalityTest, DefaultGenderIsFemale) {
    personality_generation_config_t config = personality_default_generation_config();

    // Generate multiple personalities, all should be female with default config
    for (int i = 0; i < 10; ++i) {
        config.seed = 1000 + i;  // Different seeds
        personality_profile_t profile = personality_generate_random(&config);
        EXPECT_EQ(profile.identity.gender, GENDER_FEMALE);
    }
}

//=============================================================================
// Test 12: Gender Distribution with Custom Probabilities
//=============================================================================

TEST_F(PersonalityTest, CustomGenderProbabilities) {
    personality_generation_config_t config = personality_default_generation_config();
    config.female_probability = 0.5f;
    config.male_probability = 0.5f;
    config.non_binary_probability = 0.0f;

    // Generate many personalities and check distribution
    std::map<gender_identity_t, int> counts;
    for (int i = 0; i < 100; ++i) {
        config.seed = 2000 + i;
        personality_profile_t profile = personality_generate_random(&config);
        counts[profile.identity.gender]++;
    }

    // Should have roughly 50/50 male/female distribution
    // (Allow some variance due to randomness)
    EXPECT_GT(counts[GENDER_FEMALE], 30);  // At least 30%
    EXPECT_GT(counts[GENDER_MALE], 30);    // At least 30%
}

//=============================================================================
// Test 13: NULL Pointer Handling
//=============================================================================

TEST_F(PersonalityTest, NullPointerHandling) {
    // personality_get_gender_string with NULL
    EXPECT_STREQ(personality_get_gender_string(nullptr), "Unknown");

    // personality_get_sexuality_string with NULL
    EXPECT_STREQ(personality_get_sexuality_string(nullptr), "Unknown");

    // personality_get_pronouns with NULL profile
    char subject[16], object[16], possessive[16];
    EXPECT_FALSE(personality_get_pronouns(nullptr, subject, sizeof(subject),
                                          object, sizeof(object),
                                          possessive, sizeof(possessive)));

    // personality_generate_summary with NULL profile
    char summary[1024];
    EXPECT_FALSE(personality_generate_summary(nullptr, summary, sizeof(summary)));

    // personality_create_custom with NULL traits
    identity_profile_t identity = {};
    personality_profile_t profile = personality_create_custom(nullptr, &identity);
    // Should return default random personality (fallback behavior)
    EXPECT_TRUE(is_valid_trait(profile.traits.openness));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
