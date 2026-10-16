/**
 * @file test_personality.cpp
 * @brief Unit tests for personality functionality through public API
 *
 * WHAT: Comprehensive unit tests for brain personality via brain_create_custom()
 * WHY:  Test personality generation through the public API instead of private functions
 * HOW:  GTest framework with tests using brain_create_custom() and brain_get_personality()
 *
 * REFACTORING NOTE:
 * This file was refactored from testing the private create_personality() function
 * to testing personality through the public brain creation API. This ensures:
 * - Tests compile successfully (no access to private functions)
 * - Tests verify real-world usage patterns
 * - Tests validate end-to-end personality integration with brain
 *
 * TEST CATEGORIES:
 * 1. Random personality generation (with reproducibility)
 * 2. Explicit personality specification
 * 3. NULL config handling (edge cases)
 * 4. Gender identity configuration
 * 5. Trait generation validation (Big Five bounds)
 * 6. Probability distributions
 * 7. Seed-based reproducibility
 * 8. Behavioral modifier computation
 * 9. Integration with brain lifecycle
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 2.0.0 (Refactored for public API)
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/nimcp_personality.h"

#include <cmath>
#include <cstring>

//=============================================================================
// Test Fixture
//=============================================================================

class PersonalityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data structures
    }

    void TearDown() override {
        // Cleanup if needed
    }

    // Helper: Validate trait value is in [0, 1]
    bool is_valid_trait(float value) {
        return value >= 0.0f && value <= 1.0f;
    }

    // Helper: Validate modifier is in [-0.5, 0.5]
    bool is_valid_modifier(float modifier) {
        return modifier >= -0.5f && modifier <= 0.5f;
    }

    // Helper: Create default brain config with personality settings
    brain_config_t create_default_config() {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 2;
        config.learning_rate = 0.01f;
        config.use_random_personality = true;
        config.personality_seed = 0;
        config.personality_trait_mean = 0.5f;
        config.personality_trait_stddev = 0.15f;
        config.female_probability = 1.0f;
        config.male_probability = 0.0f;
        config.non_binary_probability = 0.0f;
        strncpy(config.task_name, "test_brain", sizeof(config.task_name) - 1);
        return config;
    }

    // Helper: Create explicit personality config
    brain_config_t create_explicit_config(
        float openness, float conscientiousness, float extraversion,
        float agreeableness, float neuroticism, uint32_t gender, uint32_t sexuality) {
        brain_config_t config = create_default_config();
        config.use_random_personality = false;
        config.explicit_openness = openness;
        config.explicit_conscientiousness = conscientiousness;
        config.explicit_extraversion = extraversion;
        config.explicit_agreeableness = agreeableness;
        config.explicit_neuroticism = neuroticism;
        config.explicit_gender = gender;
        config.explicit_sexuality = sexuality;
        return config;
    }

    // Helper: Get personality from brain (accesses brain->personality)
    personality_profile_t* get_brain_personality(brain_t brain) {
        if (!brain) return nullptr;
        // Access the internal struct - this is OK for unit tests
        brain_struct* b = (brain_struct*)brain;
        return b->personality;
    }
};

//=============================================================================
// Test 1: NULL Config Handling
//=============================================================================

TEST_F(PersonalityTest, NullConfigReturnsNull) {
    /**
     * WHAT: Test guard clause for NULL config
     * WHY:  Ensure function handles invalid input gracefully
     * HOW:  Pass NULL and verify NULL return
     */
    brain_t brain = brain_create_custom(nullptr);
    EXPECT_EQ(brain, nullptr);
}

//=============================================================================
// Test 2: Random Personality Generation with Default Config
//=============================================================================

TEST_F(PersonalityTest, RandomPersonalityGenerationBasic) {
    /**
     * WHAT: Generate random personality with default settings
     * WHY:  Most common use case
     * HOW:  Create brain with random personality, verify traits generated
     */
    brain_config_t config = create_default_config();
    config.use_random_personality = true;
    config.personality_seed = 0;  // Time-based

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_TRUE(profile->was_randomly_generated);
    EXPECT_EQ(profile->seed, 0u);

    // Verify all traits are in valid range
    EXPECT_TRUE(is_valid_trait(profile->traits.openness));
    EXPECT_TRUE(is_valid_trait(profile->traits.conscientiousness));
    EXPECT_TRUE(is_valid_trait(profile->traits.extraversion));
    EXPECT_TRUE(is_valid_trait(profile->traits.agreeableness));
    EXPECT_TRUE(is_valid_trait(profile->traits.neuroticism));

    brain_destroy(brain);
}

//=============================================================================
// Test 3: Random Personality Reproducibility with Fixed Seed
//=============================================================================

TEST_F(PersonalityTest, RandomPersonalityReproducibleWithSeed) {
    /**
     * WHAT: Test seed-based reproducibility
     * WHY:  Ensure deterministic personality generation for testing
     * HOW:  Create two brains with same seed, verify identical personalities
     */
    brain_config_t config1 = create_default_config();
    config1.use_random_personality = true;
    config1.personality_seed = 42;

    brain_config_t config2 = create_default_config();
    config2.use_random_personality = true;
    config2.personality_seed = 42;

    brain_t brain1 = brain_create_custom(&config1);
    brain_t brain2 = brain_create_custom(&config2);

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);

    personality_profile_t* profile1 = get_brain_personality(brain1);
    personality_profile_t* profile2 = get_brain_personality(brain2);

    ASSERT_NE(profile1, nullptr);
    ASSERT_NE(profile2, nullptr);

    // Verify traits are identical
    EXPECT_FLOAT_EQ(profile1->traits.openness, profile2->traits.openness);
    EXPECT_FLOAT_EQ(profile1->traits.conscientiousness, profile2->traits.conscientiousness);
    EXPECT_FLOAT_EQ(profile1->traits.extraversion, profile2->traits.extraversion);
    EXPECT_FLOAT_EQ(profile1->traits.agreeableness, profile2->traits.agreeableness);
    EXPECT_FLOAT_EQ(profile1->traits.neuroticism, profile2->traits.neuroticism);

    // Verify identity is identical
    EXPECT_EQ(profile1->identity.gender, profile2->identity.gender);
    EXPECT_EQ(profile1->identity.sexuality, profile2->identity.sexuality);

    brain_destroy(brain1);
    brain_destroy(brain2);
}

//=============================================================================
// Test 4: Different Seeds Produce Different Personalities
//=============================================================================

TEST_F(PersonalityTest, DifferentSeedsProduceDifferentPersonalities) {
    /**
     * WHAT: Test that different seeds generate different personalities
     * WHY:  Ensure randomness works correctly
     * HOW:  Create two brains with different seeds, verify differences
     */
    brain_config_t config1 = create_default_config();
    config1.use_random_personality = true;
    config1.personality_seed = 42;

    brain_config_t config2 = create_default_config();
    config2.use_random_personality = true;
    config2.personality_seed = 99;

    brain_t brain1 = brain_create_custom(&config1);
    brain_t brain2 = brain_create_custom(&config2);

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);

    personality_profile_t* profile1 = get_brain_personality(brain1);
    personality_profile_t* profile2 = get_brain_personality(brain2);

    ASSERT_NE(profile1, nullptr);
    ASSERT_NE(profile2, nullptr);

    // At least one trait should differ (probability of identical = ~0)
    bool any_trait_differs = (profile1->traits.openness != profile2->traits.openness) ||
                             (profile1->traits.conscientiousness != profile2->traits.conscientiousness) ||
                             (profile1->traits.extraversion != profile2->traits.extraversion) ||
                             (profile1->traits.agreeableness != profile2->traits.agreeableness) ||
                             (profile1->traits.neuroticism != profile2->traits.neuroticism);

    EXPECT_TRUE(any_trait_differs);

    brain_destroy(brain1);
    brain_destroy(brain2);
}

//=============================================================================
// Test 5: Explicit Personality Specification
//=============================================================================

TEST_F(PersonalityTest, ExplicitPersonalitySpecification) {
    /**
     * WHAT: Test explicit personality trait specification
     * WHY:  Allow precise control for testing and specific requirements
     * HOW:  Set explicit_* fields, verify traits match
     */
    brain_config_t config = create_explicit_config(
        0.8f,   // openness: creative
        0.6f,   // conscientiousness: organized
        0.7f,   // extraversion: social
        0.9f,   // agreeableness: compassionate
        0.3f,   // neuroticism: emotionally stable
        GENDER_FEMALE,
        SEXUALITY_BISEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_FALSE(profile->was_randomly_generated);

    // Verify traits match specification (may be clamped)
    EXPECT_FLOAT_EQ(profile->traits.openness, 0.8f);
    EXPECT_FLOAT_EQ(profile->traits.conscientiousness, 0.6f);
    EXPECT_FLOAT_EQ(profile->traits.extraversion, 0.7f);
    EXPECT_FLOAT_EQ(profile->traits.agreeableness, 0.9f);
    EXPECT_FLOAT_EQ(profile->traits.neuroticism, 0.3f);

    // Verify identity
    EXPECT_EQ(profile->identity.gender, GENDER_FEMALE);
    EXPECT_EQ(profile->identity.sexuality, SEXUALITY_BISEXUAL);

    brain_destroy(brain);
}

//=============================================================================
// Test 6: Clamping of Out-of-Range Traits
//=============================================================================

TEST_F(PersonalityTest, ClampingOfOutOfRangeTraits) {
    /**
     * WHAT: Test that out-of-range traits are clamped to [0, 1]
     * WHY:  Invalid input should be made safe, not cause errors
     * HOW:  Pass traits > 1.0 and < 0.0, verify clamping
     */
    brain_config_t config = create_explicit_config(
        1.5f,   // should clamp to 1.0
        -0.3f,  // should clamp to 0.0
        0.5f,
        0.5f,
        0.5f,
        GENDER_MALE,
        SEXUALITY_HETEROSEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_FLOAT_EQ(profile->traits.openness, 1.0f);
    EXPECT_FLOAT_EQ(profile->traits.conscientiousness, 0.0f);

    brain_destroy(brain);
}

//=============================================================================
// Test 7: Behavioral Modifiers Computed Correctly
//=============================================================================

TEST_F(PersonalityTest, BehavioralModifiersComputed) {
    /**
     * WHAT: Test that behavioral modifiers are correctly computed from traits
     * WHY:  Modifiers drive personality effects on brain behavior
     * HOW:  Verify modifier = trait - 0.5 formula
     */
    brain_config_t config = create_explicit_config(
        0.8f,   // openness
        0.6f,   // conscientiousness
        0.7f,   // extraversion
        0.9f,   // agreeableness
        0.3f,   // neuroticism
        GENDER_FEMALE,
        SEXUALITY_HETEROSEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);

    // Verify modifier formula: modifier = trait - 0.5
    EXPECT_FLOAT_EQ(profile->curiosity_modifier, 0.8f - 0.5f);
    EXPECT_FLOAT_EQ(profile->planning_modifier, 0.6f - 0.5f);
    EXPECT_FLOAT_EQ(profile->social_drive_modifier, 0.7f - 0.5f);
    EXPECT_FLOAT_EQ(profile->empathy_modifier, 0.9f - 0.5f);
    EXPECT_FLOAT_EQ(profile->stress_sensitivity_modifier, 0.3f - 0.5f);

    // Verify all modifiers are in valid range
    EXPECT_TRUE(is_valid_modifier(profile->curiosity_modifier));
    EXPECT_TRUE(is_valid_modifier(profile->planning_modifier));
    EXPECT_TRUE(is_valid_modifier(profile->social_drive_modifier));
    EXPECT_TRUE(is_valid_modifier(profile->empathy_modifier));
    EXPECT_TRUE(is_valid_modifier(profile->stress_sensitivity_modifier));

    brain_destroy(brain);
}

//=============================================================================
// Test 8: Gender Identity Configuration - Female
//=============================================================================

TEST_F(PersonalityTest, GenderIdentityFemale) {
    /**
     * WHAT: Test female gender identity configuration
     * WHY:  Default gender should be female
     * HOW:  Create explicit config with female, verify identity
     */
    brain_config_t config = create_explicit_config(
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        GENDER_FEMALE,
        SEXUALITY_HETEROSEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->identity.gender, GENDER_FEMALE);

    // Verify gender string
    const char* gender_str = personality_get_gender_string(profile);
    EXPECT_STREQ(gender_str, "Female");

    brain_destroy(brain);
}

//=============================================================================
// Test 9: Gender Identity Configuration - Male
//=============================================================================

TEST_F(PersonalityTest, GenderIdentityMale) {
    /**
     * WHAT: Test male gender identity configuration
     * WHY:  Allow male identity specification
     * HOW:  Create explicit config with male, verify identity
     */
    brain_config_t config = create_explicit_config(
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        GENDER_MALE,
        SEXUALITY_HETEROSEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->identity.gender, GENDER_MALE);

    // Verify gender string
    const char* gender_str = personality_get_gender_string(profile);
    EXPECT_STREQ(gender_str, "Male");

    brain_destroy(brain);
}

//=============================================================================
// Test 10: Gender Identity Configuration - Non-Binary
//=============================================================================

TEST_F(PersonalityTest, GenderIdentityNonBinary) {
    /**
     * WHAT: Test non-binary gender identity configuration
     * WHY:  Support diverse gender identities
     * HOW:  Create explicit config with non-binary, verify identity
     */
    brain_config_t config = create_explicit_config(
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        GENDER_NON_BINARY,
        SEXUALITY_PANSEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->identity.gender, GENDER_NON_BINARY);

    // Verify gender string
    const char* gender_str = personality_get_gender_string(profile);
    EXPECT_STREQ(gender_str, "Non-Binary");

    brain_destroy(brain);
}

//=============================================================================
// Test 11: Sexuality Configuration - Heterosexual
//=============================================================================

TEST_F(PersonalityTest, SexualityHeterosexual) {
    /**
     * WHAT: Test heterosexual orientation configuration
     * WHY:  Support all sexual orientations
     * HOW:  Create explicit config, verify sexuality
     */
    brain_config_t config = create_explicit_config(
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        GENDER_FEMALE,
        SEXUALITY_HETEROSEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->identity.sexuality, SEXUALITY_HETEROSEXUAL);

    // Verify sexuality string
    const char* sexuality_str = personality_get_sexuality_string(profile);
    EXPECT_STREQ(sexuality_str, "Heterosexual");

    brain_destroy(brain);
}

//=============================================================================
// Test 12: Sexuality Configuration - Homosexual
//=============================================================================

TEST_F(PersonalityTest, SexualityHomosexual) {
    /**
     * WHAT: Test homosexual orientation configuration
     * WHY:  Support all sexual orientations
     * HOW:  Create explicit config, verify sexuality
     */
    brain_config_t config = create_explicit_config(
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        GENDER_MALE,
        SEXUALITY_HOMOSEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->identity.sexuality, SEXUALITY_HOMOSEXUAL);

    // Verify sexuality string
    const char* sexuality_str = personality_get_sexuality_string(profile);
    EXPECT_STREQ(sexuality_str, "Homosexual");

    brain_destroy(brain);
}

//=============================================================================
// Test 13: Sexuality Configuration - Bisexual
//=============================================================================

TEST_F(PersonalityTest, SexualityBisexual) {
    /**
     * WHAT: Test bisexual orientation configuration
     * WHY:  Support all sexual orientations
     * HOW:  Create explicit config, verify sexuality
     */
    brain_config_t config = create_explicit_config(
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        GENDER_FEMALE,
        SEXUALITY_BISEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->identity.sexuality, SEXUALITY_BISEXUAL);

    // Verify sexuality string
    const char* sexuality_str = personality_get_sexuality_string(profile);
    EXPECT_STREQ(sexuality_str, "Bisexual");

    brain_destroy(brain);
}

//=============================================================================
// Test 14: Probability Distribution - Female Probability
//=============================================================================

TEST_F(PersonalityTest, FemaleOnlyDistribution) {
    /**
     * WHAT: Test that 100% female_probability generates females
     * WHY:  Probability distribution should be respected
     * HOW:  Set female_probability=1.0, male_probability=0.0, verify all female
     */
    brain_config_t config = create_default_config();
    config.use_random_personality = true;
    config.personality_seed = 12345;
    config.female_probability = 1.0f;
    config.male_probability = 0.0f;
    config.non_binary_probability = 0.0f;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->identity.gender, GENDER_FEMALE);

    brain_destroy(brain);
}

//=============================================================================
// Test 15: Probability Distribution - Male Probability
//=============================================================================

TEST_F(PersonalityTest, MaleOnlyDistribution) {
    /**
     * WHAT: Test that 100% male_probability generates males
     * WHY:  Probability distribution should be respected
     * HOW:  Set male_probability=1.0, female_probability=0.0, verify all male
     */
    brain_config_t config = create_default_config();
    config.use_random_personality = true;
    config.personality_seed = 54321;
    config.female_probability = 0.0f;
    config.male_probability = 1.0f;
    config.non_binary_probability = 0.0f;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile->identity.gender, GENDER_MALE);

    brain_destroy(brain);
}

//=============================================================================
// Test 16: Trait Mean and Standard Deviation Configuration
//=============================================================================

TEST_F(PersonalityTest, TraitMeanAndStddevConfiguration) {
    /**
     * WHAT: Test that trait_mean and trait_stddev are used in random generation
     * WHY:  Allow control over personality distribution
     * HOW:  Generate personality, verify statistical properties
     */
    brain_config_t config = create_default_config();
    config.use_random_personality = true;
    config.personality_seed = 99999;
    config.personality_trait_mean = 0.8f;   // High mean
    config.personality_trait_stddev = 0.05f;  // Low stddev

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);

    // With high mean and low stddev, traits should generally be high
    float avg_trait = (profile->traits.openness +
                       profile->traits.conscientiousness +
                       profile->traits.extraversion +
                       profile->traits.agreeableness +
                       profile->traits.neuroticism) / 5.0f;

    // Average should be closer to 0.8 than 0.5
    EXPECT_GT(avg_trait, 0.6f);

    brain_destroy(brain);
}

//=============================================================================
// Test 17: Timestamp and Seed Metadata
//=============================================================================

TEST_F(PersonalityTest, TimestampAndSeedMetadata) {
    /**
     * WHAT: Test that created_timestamp_ms and seed are set correctly
     * WHY:  Metadata should be accurate for reproducibility
     * HOW:  Create personality, verify timestamp and seed fields
     */
    brain_config_t config = create_default_config();
    config.use_random_personality = true;
    config.personality_seed = 777;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_GT(profile->created_timestamp_ms, 0u);
    EXPECT_EQ(profile->seed, 777u);

    brain_destroy(brain);
}

//=============================================================================
// Test 18: All Traits in Extreme Ranges
//=============================================================================

TEST_F(PersonalityTest, ExtremeTraitRanges) {
    /**
     * WHAT: Test personality with extreme trait values
     * WHY:  Edge cases should be handled correctly
     * HOW:  Create personalities with 0.0 and 1.0 trait values
     */
    // All low traits
    brain_config_t config_low = create_explicit_config(
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        GENDER_FEMALE, SEXUALITY_HETEROSEXUAL
    );

    brain_t brain_low = brain_create_custom(&config_low);
    ASSERT_NE(brain_low, nullptr);

    personality_profile_t* profile_low = get_brain_personality(brain_low);
    ASSERT_NE(profile_low, nullptr);
    EXPECT_FLOAT_EQ(profile_low->curiosity_modifier, -0.5f);
    EXPECT_FLOAT_EQ(profile_low->planning_modifier, -0.5f);
    EXPECT_FLOAT_EQ(profile_low->social_drive_modifier, -0.5f);
    EXPECT_FLOAT_EQ(profile_low->empathy_modifier, -0.5f);
    EXPECT_FLOAT_EQ(profile_low->stress_sensitivity_modifier, -0.5f);
    brain_destroy(brain_low);

    // All high traits
    brain_config_t config_high = create_explicit_config(
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        GENDER_MALE, SEXUALITY_HOMOSEXUAL
    );

    brain_t brain_high = brain_create_custom(&config_high);
    ASSERT_NE(brain_high, nullptr);

    personality_profile_t* profile_high = get_brain_personality(brain_high);
    ASSERT_NE(profile_high, nullptr);
    EXPECT_FLOAT_EQ(profile_high->curiosity_modifier, 0.5f);
    EXPECT_FLOAT_EQ(profile_high->planning_modifier, 0.5f);
    EXPECT_FLOAT_EQ(profile_high->social_drive_modifier, 0.5f);
    EXPECT_FLOAT_EQ(profile_high->empathy_modifier, 0.5f);
    EXPECT_FLOAT_EQ(profile_high->stress_sensitivity_modifier, 0.5f);
    brain_destroy(brain_high);
}

//=============================================================================
// Test 19: Big Five Trait Independence
//=============================================================================

TEST_F(PersonalityTest, BigFiveTraitIndependence) {
    /**
     * WHAT: Test that Big Five traits are independent
     * WHY:  Each trait should be generated/configured separately
     * HOW:  Create personality, verify each trait can have different values
     */
    brain_config_t config = create_explicit_config(
        0.1f, 0.3f, 0.5f, 0.7f, 0.9f,  // Different values for each
        GENDER_FEMALE, SEXUALITY_BISEXUAL
    );

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_NE(profile->traits.openness, profile->traits.conscientiousness);
    EXPECT_NE(profile->traits.conscientiousness, profile->traits.extraversion);
    EXPECT_NE(profile->traits.extraversion, profile->traits.agreeableness);
    EXPECT_NE(profile->traits.agreeableness, profile->traits.neuroticism);

    brain_destroy(brain);
}

//=============================================================================
// Test 20: Config with All Default Values
//=============================================================================

TEST_F(PersonalityTest, AllDefaultValuesInConfig) {
    /**
     * WHAT: Test personality creation with all config fields at defaults
     * WHY:  Should work with minimal configuration
     * HOW:  Create personality from default config, verify valid result
     */
    brain_config_t config = create_default_config();
    // All fields already at defaults

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    personality_profile_t* profile = get_brain_personality(brain);
    ASSERT_NE(profile, nullptr);
    EXPECT_TRUE(profile->was_randomly_generated);
    EXPECT_TRUE(is_valid_trait(profile->traits.openness));
    EXPECT_TRUE(is_valid_trait(profile->traits.conscientiousness));
    EXPECT_TRUE(is_valid_trait(profile->traits.extraversion));
    EXPECT_TRUE(is_valid_trait(profile->traits.agreeableness));
    EXPECT_TRUE(is_valid_trait(profile->traits.neuroticism));

    brain_destroy(brain);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
