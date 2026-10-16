//=============================================================================
// test_lateralization_regression.cpp - Lateralization Regression Tests
//=============================================================================
/**
 * @file test_lateralization_regression.cpp
 * @brief Regression tests for lateralization and hemisphere specialization
 *
 * WHAT: Tests for dominance calculation, plasticity, domain routing
 * WHY:  Ensure lateralization behavior is stable across versions
 * HOW:  GTest framework with accuracy and determinism checks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>

#include "utils/nimcp_test_base.h"


#include "core/brain/hemispheric/nimcp_lateralization.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class LateralizationRegressionTest : public NimcpTestBase {
protected:
    lateralization_profile_t profile;

    void SetUp() override {
        NimcpTestBase::SetUp();
        profile = lateralization_default_profile();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Dominance Calculation Accuracy Tests
//=============================================================================

TEST_F(LateralizationRegressionTest, DefaultLanguageDominance) {
    // Language should be strongly left-dominant (0.95)
    float dom = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_NEAR(dom, 0.95f, 0.01f);

    hemisphere_id_t dominant = lateralization_get_dominant_hemisphere(
        &profile, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_EQ(dominant, HEMISPHERE_LEFT);

    EXPECT_TRUE(lateralization_is_strongly_lateralized(&profile, COGNITIVE_DOMAIN_LANGUAGE));
}

TEST_F(LateralizationRegressionTest, DefaultSpatialDominance) {
    // Spatial should be right-dominant (0.20)
    float dom = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_SPATIAL);
    EXPECT_NEAR(dom, 0.20f, 0.01f);

    hemisphere_id_t dominant = lateralization_get_dominant_hemisphere(
        &profile, COGNITIVE_DOMAIN_SPATIAL);
    EXPECT_EQ(dominant, HEMISPHERE_RIGHT);

    EXPECT_TRUE(lateralization_is_strongly_lateralized(&profile, COGNITIVE_DOMAIN_SPATIAL));
}

TEST_F(LateralizationRegressionTest, DefaultMotorGrossBilateral) {
    // Gross motor should be bilateral (0.50)
    float dom = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_MOTOR_GROSS);
    EXPECT_NEAR(dom, 0.50f, 0.01f);

    // Not strongly lateralized
    EXPECT_FALSE(lateralization_is_strongly_lateralized(&profile, COGNITIVE_DOMAIN_MOTOR_GROSS));
}

TEST_F(LateralizationRegressionTest, DefaultFaceRecognitionRightDominant) {
    // Face recognition strongly right (0.15)
    float dom = lateralization_get_dominance(&profile, COGNITIVE_DOMAIN_FACE_RECOGNITION);
    EXPECT_NEAR(dom, 0.15f, 0.01f);

    hemisphere_id_t dominant = lateralization_get_dominant_hemisphere(
        &profile, COGNITIVE_DOMAIN_FACE_RECOGNITION);
    EXPECT_EQ(dominant, HEMISPHERE_RIGHT);

    EXPECT_TRUE(lateralization_is_strongly_lateralized(&profile, COGNITIVE_DOMAIN_FACE_RECOGNITION));
}

TEST_F(LateralizationRegressionTest, AllDomainsHaveValidDominance) {
    for (int d = 0; d < COGNITIVE_DOMAIN_COUNT; d++) {
        float dom = lateralization_get_dominance(&profile, (cognitive_domain_t)d);
        EXPECT_GE(dom, 0.0f) << "Domain " << d << " has negative dominance";
        EXPECT_LE(dom, 1.0f) << "Domain " << d << " has dominance > 1";
    }
}

TEST_F(LateralizationRegressionTest, DominanceThresholdCorrect) {
    // Test boundary cases for dominant hemisphere determination
    profile.language_dominance = 0.51f;
    EXPECT_EQ(lateralization_get_dominant_hemisphere(&profile, COGNITIVE_DOMAIN_LANGUAGE),
              HEMISPHERE_LEFT);

    profile.language_dominance = 0.49f;
    EXPECT_EQ(lateralization_get_dominant_hemisphere(&profile, COGNITIVE_DOMAIN_LANGUAGE),
              HEMISPHERE_RIGHT);

    profile.language_dominance = 0.50f;
    // Exactly 0.5 - implementation may favor one side
    hemisphere_id_t dom = lateralization_get_dominant_hemisphere(
        &profile, COGNITIVE_DOMAIN_LANGUAGE);
    EXPECT_TRUE(dom == HEMISPHERE_LEFT || dom == HEMISPHERE_RIGHT);
}

//=============================================================================
// Plasticity Shift Rate Consistency Tests
//=============================================================================

TEST_F(LateralizationRegressionTest, PlasticityShiftPositive) {
    ASSERT_TRUE(profile.enable_plasticity);

    /* Use spatial domain (0.20) which has room to shift positive,
     * since language_dominance (0.95) == max_dominance (0.95) and
     * cannot shift further positive. */
    float initial = profile.spatial_dominance;
    lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_SPATIAL, 0.05f);

    EXPECT_GT(profile.spatial_dominance, initial);
    EXPECT_LE(profile.spatial_dominance, profile.max_dominance);
}

TEST_F(LateralizationRegressionTest, PlasticityShiftNegative) {
    ASSERT_TRUE(profile.enable_plasticity);

    float initial = profile.language_dominance;
    lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE, -0.05f);

    EXPECT_LT(profile.language_dominance, initial);
    EXPECT_GE(profile.language_dominance, profile.min_dominance);
}

TEST_F(LateralizationRegressionTest, PlasticityClampedToMax) {
    ASSERT_TRUE(profile.enable_plasticity);

    // Shift way positive
    lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE, 1.0f);

    EXPECT_LE(profile.language_dominance, profile.max_dominance);
}

TEST_F(LateralizationRegressionTest, PlasticityClampedToMin) {
    ASSERT_TRUE(profile.enable_plasticity);

    // Shift way negative
    lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE, -1.0f);

    EXPECT_GE(profile.language_dominance, profile.min_dominance);
}

TEST_F(LateralizationRegressionTest, PlasticityDisabledNoChange) {
    profile.enable_plasticity = false;

    float initial = profile.language_dominance;
    int result = lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE, 0.1f);

    EXPECT_EQ(result, -1);  // Should fail when disabled
    EXPECT_FLOAT_EQ(profile.language_dominance, initial);
}

TEST_F(LateralizationRegressionTest, UsagePlasticityLeftHemisphere) {
    ASSERT_TRUE(profile.enable_plasticity);

    float initial = profile.spatial_dominance;

    // Repeated left hemisphere usage should shift toward left
    for (int i = 0; i < 100; i++) {
        lateralization_apply_usage_plasticity(&profile, COGNITIVE_DOMAIN_SPATIAL,
            HEMISPHERE_LEFT);
    }

    EXPECT_GT(profile.spatial_dominance, initial);
}

TEST_F(LateralizationRegressionTest, UsagePlasticityRightHemisphere) {
    ASSERT_TRUE(profile.enable_plasticity);

    float initial = profile.language_dominance;

    // Repeated right hemisphere usage should shift toward right
    for (int i = 0; i < 100; i++) {
        lateralization_apply_usage_plasticity(&profile, COGNITIVE_DOMAIN_LANGUAGE,
            HEMISPHERE_RIGHT);
    }

    EXPECT_LT(profile.language_dominance, initial);
}

TEST_F(LateralizationRegressionTest, PlasticityRateAffectsShiftMagnitude) {
    lateralization_profile_t fast_profile = lateralization_default_profile();
    lateralization_profile_t slow_profile = lateralization_default_profile();

    fast_profile.plasticity_rate = 0.01f;
    slow_profile.plasticity_rate = 0.001f;

    float fast_initial = fast_profile.spatial_dominance;
    float slow_initial = slow_profile.spatial_dominance;

    // Same number of plasticity events
    for (int i = 0; i < 50; i++) {
        lateralization_apply_usage_plasticity(&fast_profile, COGNITIVE_DOMAIN_SPATIAL,
            HEMISPHERE_LEFT);
        lateralization_apply_usage_plasticity(&slow_profile, COGNITIVE_DOMAIN_SPATIAL,
            HEMISPHERE_LEFT);
    }

    float fast_change = fast_profile.spatial_dominance - fast_initial;
    float slow_change = slow_profile.spatial_dominance - slow_initial;

    // Fast rate should produce larger change
    EXPECT_GT(std::abs(fast_change), std::abs(slow_change));
}

//=============================================================================
// Domain Routing Determinism Tests
//=============================================================================

TEST_F(LateralizationRegressionTest, RoutingDeterministicSameProfile) {
    // Same profile should always route to same hemisphere
    for (int d = 0; d < COGNITIVE_DOMAIN_COUNT; d++) {
        hemisphere_id_t first = lateralization_get_dominant_hemisphere(
            &profile, (cognitive_domain_t)d);

        for (int i = 0; i < 100; i++) {
            hemisphere_id_t current = lateralization_get_dominant_hemisphere(
                &profile, (cognitive_domain_t)d);
            EXPECT_EQ(current, first) << "Domain " << d << " routing changed";
        }
    }
}

TEST_F(LateralizationRegressionTest, RoutingConsistentAcrossProfiles) {
    lateralization_profile_t profile1 = lateralization_default_profile();
    lateralization_profile_t profile2 = lateralization_default_profile();

    for (int d = 0; d < COGNITIVE_DOMAIN_COUNT; d++) {
        hemisphere_id_t dom1 = lateralization_get_dominant_hemisphere(
            &profile1, (cognitive_domain_t)d);
        hemisphere_id_t dom2 = lateralization_get_dominant_hemisphere(
            &profile2, (cognitive_domain_t)d);

        EXPECT_EQ(dom1, dom2) << "Domain " << d << " differs between identical profiles";
    }
}

//=============================================================================
// Handedness Effects Verification Tests
//=============================================================================

TEST_F(LateralizationRegressionTest, RightHandedMotorFine) {
    lateralization_profile_t rh = lateralization_default_profile();
    EXPECT_EQ(rh.handedness, HANDEDNESS_RIGHT);

    // Right-handed: fine motor left-dominant (controls right hand)
    EXPECT_GT(rh.motor_fine_dominance, 0.5f);
    EXPECT_EQ(lateralization_get_dominant_hemisphere(&rh, COGNITIVE_DOMAIN_MOTOR_FINE),
              HEMISPHERE_LEFT);
}

TEST_F(LateralizationRegressionTest, LeftHandedMotorFine) {
    lateralization_profile_t lh = lateralization_left_handed_profile();
    EXPECT_EQ(lh.handedness, HANDEDNESS_LEFT);

    // Left-handed: fine motor right-dominant (controls left hand)
    EXPECT_LT(lh.motor_fine_dominance, 0.5f);
    EXPECT_EQ(lateralization_get_dominant_hemisphere(&lh, COGNITIVE_DOMAIN_MOTOR_FINE),
              HEMISPHERE_RIGHT);
}

TEST_F(LateralizationRegressionTest, BilateralProfileSymmetric) {
    lateralization_profile_t bi = lateralization_bilateral_profile();
    EXPECT_EQ(bi.handedness, HANDEDNESS_AMBIDEXTROUS);

    // All domains should be at 0.5
    for (int d = 0; d < COGNITIVE_DOMAIN_COUNT; d++) {
        float dom = lateralization_get_dominance(&bi, (cognitive_domain_t)d);
        EXPECT_FLOAT_EQ(dom, 0.5f) << "Domain " << d << " not bilateral";
    }
}

TEST_F(LateralizationRegressionTest, LeftHandedLanguageMoreBilateral) {
    lateralization_profile_t rh = lateralization_default_profile();
    lateralization_profile_t lh = lateralization_left_handed_profile();

    // Left-handers typically have less lateralized language
    // Language should be less left-dominant in left-handers
    EXPECT_LE(lh.language_dominance, rh.language_dominance);
}

TEST_F(LateralizationRegressionTest, ResetRestoresHandednessDefaults) {
    lateralization_profile_t p = lateralization_default_profile();

    // Modify
    p.language_dominance = 0.3f;
    p.spatial_dominance = 0.8f;

    // Reset to right-handed
    lateralization_reset(&p, HANDEDNESS_RIGHT);

    lateralization_profile_t expected = lateralization_default_profile();
    EXPECT_FLOAT_EQ(p.language_dominance, expected.language_dominance);
    EXPECT_FLOAT_EQ(p.spatial_dominance, expected.spatial_dominance);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(LateralizationRegressionTest, DefaultProfileValuesStable) {
    lateralization_profile_t p = lateralization_default_profile();

    // These specific values should be stable across versions
    EXPECT_FLOAT_EQ(p.language_dominance, 0.95f);
    EXPECT_FLOAT_EQ(p.spatial_dominance, 0.20f);
    EXPECT_FLOAT_EQ(p.motor_fine_dominance, 0.90f);
    EXPECT_FLOAT_EQ(p.motor_gross_dominance, 0.50f);
    EXPECT_FLOAT_EQ(p.emotion_processing_dominance, 0.30f);
    EXPECT_FLOAT_EQ(p.face_recognition_dominance, 0.15f);
    EXPECT_FLOAT_EQ(p.logical_reasoning_dominance, 0.85f);
    EXPECT_FLOAT_EQ(p.creative_thinking_dominance, 0.35f);

    EXPECT_EQ(p.handedness, HANDEDNESS_RIGHT);
    EXPECT_TRUE(p.enable_plasticity);
}

TEST_F(LateralizationRegressionTest, DomainNamesStable) {
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_LANGUAGE), "Language");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_SPATIAL), "Spatial");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_MOTOR_FINE), "Fine Motor");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_MOTOR_GROSS), "Gross Motor");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_EMOTION), "Emotion Processing");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_ATTENTION_GLOBAL), "Global Attention");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_ATTENTION_LOCAL), "Local Attention");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_FACE_RECOGNITION), "Face Recognition");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_LOGICAL_REASONING), "Logical Reasoning");
    EXPECT_STREQ(cognitive_domain_name(COGNITIVE_DOMAIN_CREATIVE_THINKING), "Creative Thinking");
}

TEST_F(LateralizationRegressionTest, HemisphereNamesStable) {
    EXPECT_STREQ(hemisphere_name(HEMISPHERE_LEFT), "Left");
    EXPECT_STREQ(hemisphere_name(HEMISPHERE_RIGHT), "Right");
}

TEST_F(LateralizationRegressionTest, HandednessNamesStable) {
    EXPECT_STREQ(handedness_name(HANDEDNESS_RIGHT), "Right-handed");
    EXPECT_STREQ(handedness_name(HANDEDNESS_LEFT), "Left-handed");
    EXPECT_STREQ(handedness_name(HANDEDNESS_AMBIDEXTROUS), "Ambidextrous");
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(LateralizationRegressionTest, ValidProfilePasses) {
    EXPECT_TRUE(lateralization_validate(&profile));
}

TEST_F(LateralizationRegressionTest, InvalidDominanceFails) {
    profile.language_dominance = 1.5f;  // Invalid
    EXPECT_FALSE(lateralization_validate(&profile));

    profile.language_dominance = -0.1f;  // Invalid
    EXPECT_FALSE(lateralization_validate(&profile));
}

TEST_F(LateralizationRegressionTest, InvalidPlasticityRangeFails) {
    profile.min_dominance = 0.5f;
    profile.max_dominance = 0.3f;  // Max < min
    EXPECT_FALSE(lateralization_validate(&profile));
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(LateralizationRegressionTest, ManyPlasticityShiftsStable) {
    for (int i = 0; i < 10000; i++) {
        float shift = (i % 2 == 0) ? 0.001f : -0.001f;
        lateralization_shift_dominance(&profile, COGNITIVE_DOMAIN_LANGUAGE, shift);

        // Should never go out of bounds
        EXPECT_GE(profile.language_dominance, profile.min_dominance);
        EXPECT_LE(profile.language_dominance, profile.max_dominance);
    }
}

TEST_F(LateralizationRegressionTest, AllDomainsPlasticityStable) {
    for (int d = 0; d < COGNITIVE_DOMAIN_COUNT; d++) {
        lateralization_profile_t p = lateralization_default_profile();

        for (int i = 0; i < 1000; i++) {
            lateralization_apply_usage_plasticity(&p, (cognitive_domain_t)d,
                (i % 2 == 0) ? HEMISPHERE_LEFT : HEMISPHERE_RIGHT);
        }

        // Validate profile still valid
        EXPECT_TRUE(lateralization_validate(&p)) << "Domain " << d << " made profile invalid";
    }
}
