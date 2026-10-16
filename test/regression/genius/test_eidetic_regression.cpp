/**
 * @file test_eidetic_regression.cpp
 * @brief Regression tests for eidetic memory configuration consistency
 *
 * Test Categories:
 * 1. Preset Consistency - Preset configs don't change between versions
 * 2. Validation Consistency - Validation rules remain stable
 * 3. Utility Function Consistency - Scale/decay computations remain stable
 * 4. Error Code Consistency - Error codes and strings remain stable
 * 5. Field Bounds Consistency - Config field bounds remain stable
 *
 * @author NIMCP Development Team
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/genius/eidetic/nimcp_eidetic_memory.h"
#include "core/brain/genius/nimcp_genius_traits.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class EideticRegressionTest : public ::testing::Test {
protected:
    // Helper for floating point comparison with tolerance
    static bool FloatEqual(float a, float b, float epsilon = 0.001f) {
        return std::fabs(a - b) < epsilon;
    }
};

//=============================================================================
// 1. PRESET CONSISTENCY TESTS
//=============================================================================

TEST_F(EideticRegressionTest, TeslaPresetVisualStrength) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);

    // Tesla's visual eidetic should always be >= 2.5 (exceptional)
    EXPECT_GE(tesla->visual_eidetic, 2.5f);
    EXPECT_LE(tesla->visual_eidetic, 3.0f);
}

TEST_F(EideticRegressionTest, TeslaPresetSpatialStrength) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);

    // Tesla's spatial eidetic should always be >= 2.5 (exceptional)
    EXPECT_GE(tesla->spatial_eidetic, 2.5f);
    EXPECT_LE(tesla->spatial_eidetic, 3.0f);
}

TEST_F(EideticRegressionTest, MozartPresetAuditoryStrength) {
    const eidetic_memory_config_t* mozart = eidetic_config_mozart();
    ASSERT_NE(mozart, nullptr);

    // Mozart's auditory eidetic should always be >= 2.5 (exceptional)
    EXPECT_GE(mozart->auditory_eidetic, 2.5f);
    EXPECT_LE(mozart->auditory_eidetic, 3.0f);
}

TEST_F(EideticRegressionTest, VonNeumannPresetVerbalStrength) {
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    ASSERT_NE(vn, nullptr);

    // von Neumann's verbal eidetic should always be >= 2.5 (exceptional)
    EXPECT_GE(vn->verbal_eidetic, 2.5f);
    EXPECT_LE(vn->verbal_eidetic, 3.0f);
}

TEST_F(EideticRegressionTest, KimPeekPresetDetailGranularity) {
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    ASSERT_NE(kp, nullptr);

    // Kim Peek's detail granularity should be exceptional
    EXPECT_GE(kp->detail_granularity, 2.0f);
}

TEST_F(EideticRegressionTest, WiltshirePresetVisualStrength) {
    const eidetic_memory_config_t* sw = eidetic_config_wiltshire();
    ASSERT_NE(sw, nullptr);

    // Wiltshire's visual eidetic should be exceptional
    EXPECT_GE(sw->visual_eidetic, 2.5f);
    EXPECT_LE(sw->visual_eidetic, 3.0f);
}

TEST_F(EideticRegressionTest, AllPresetsNonNull) {
    // All preset functions should always return non-null
    EXPECT_NE(eidetic_config_tesla(), nullptr);
    EXPECT_NE(eidetic_config_mozart(), nullptr);
    EXPECT_NE(eidetic_config_vonneumann(), nullptr);
    EXPECT_NE(eidetic_config_kim_peek(), nullptr);
    EXPECT_NE(eidetic_config_wiltshire(), nullptr);
}

TEST_F(EideticRegressionTest, AllPresetsValid) {
    // All presets should always pass validation
    EXPECT_TRUE(eidetic_config_is_valid(eidetic_config_tesla()));
    EXPECT_TRUE(eidetic_config_is_valid(eidetic_config_mozart()));
    EXPECT_TRUE(eidetic_config_is_valid(eidetic_config_vonneumann()));
    EXPECT_TRUE(eidetic_config_is_valid(eidetic_config_kim_peek()));
    EXPECT_TRUE(eidetic_config_is_valid(eidetic_config_wiltshire()));
}

//=============================================================================
// 2. VALIDATION CONSISTENCY TESTS
//=============================================================================

TEST_F(EideticRegressionTest, ValidationRejectsNullConfig) {
    EXPECT_FALSE(eidetic_config_is_valid(nullptr));
}

TEST_F(EideticRegressionTest, ValidationRejectsNegativeVisual) {
    eidetic_memory_config_t config;
    memcpy(&config, eidetic_config_tesla(), sizeof(config));
    config.visual_eidetic = -0.1f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticRegressionTest, ValidationRejectsExcessiveVisual) {
    eidetic_memory_config_t config;
    memcpy(&config, eidetic_config_tesla(), sizeof(config));
    config.visual_eidetic = 3.1f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticRegressionTest, ValidationRejectsNegativeAuditory) {
    eidetic_memory_config_t config;
    memcpy(&config, eidetic_config_mozart(), sizeof(config));
    config.auditory_eidetic = -0.5f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticRegressionTest, ValidationRejectsExcessiveSpatial) {
    eidetic_memory_config_t config;
    memcpy(&config, eidetic_config_tesla(), sizeof(config));
    config.spatial_eidetic = 4.0f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticRegressionTest, ValidationRejectsNegativeEncodingSpeed) {
    eidetic_memory_config_t config;
    memcpy(&config, eidetic_config_tesla(), sizeof(config));
    config.encoding_speed = -1.0f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticRegressionTest, ValidationRejectsInvalidRetrievalAccuracy) {
    eidetic_memory_config_t config;
    memcpy(&config, eidetic_config_tesla(), sizeof(config));
    config.retrieval_accuracy = 1.5f;  // > 1.0
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticRegressionTest, ValidationAcceptsBoundaryValues) {
    eidetic_memory_config_t config;
    memcpy(&config, eidetic_config_tesla(), sizeof(config));

    // Test boundary values
    config.visual_eidetic = 0.0f;
    config.auditory_eidetic = 3.0f;
    config.spatial_eidetic = 0.0f;
    config.verbal_eidetic = 3.0f;
    config.retrieval_accuracy = 0.0f;

    EXPECT_TRUE(eidetic_config_is_valid(&config));

    config.retrieval_accuracy = 1.0f;
    EXPECT_TRUE(eidetic_config_is_valid(&config));
}

//=============================================================================
// 3. UTILITY FUNCTION CONSISTENCY TESTS
//=============================================================================

TEST_F(EideticRegressionTest, ScaleValueAtZeroIsUnchanged) {
    // At strength 0, value should remain unchanged
    EXPECT_TRUE(FloatEqual(eidetic_scale_value(100.0f, 0.0f, 4.0f), 100.0f));
    EXPECT_TRUE(FloatEqual(eidetic_scale_value(50.0f, 0.0f, 2.0f), 50.0f));
    EXPECT_TRUE(FloatEqual(eidetic_scale_value(1.0f, 0.0f, 10.0f), 1.0f));
}

TEST_F(EideticRegressionTest, ScaleValueAtMaxIsFullyScaled) {
    // At strength 3.0, value should be multiplied by max_multiplier
    EXPECT_TRUE(FloatEqual(eidetic_scale_value(100.0f, 3.0f, 4.0f), 400.0f));
    EXPECT_TRUE(FloatEqual(eidetic_scale_value(50.0f, 3.0f, 2.0f), 100.0f));
    EXPECT_TRUE(FloatEqual(eidetic_scale_value(10.0f, 3.0f, 8.0f), 80.0f));
}

TEST_F(EideticRegressionTest, ScaleValueLinearInterpolation) {
    // At strength 1.5 (half of 3.0), multiplier should be halfway to max
    // For max_multiplier=4.0: multiplier = 1.0 + (4.0-1.0)*0.5 = 2.5
    float result = eidetic_scale_value(100.0f, 1.5f, 4.0f);
    EXPECT_TRUE(FloatEqual(result, 250.0f));
}

TEST_F(EideticRegressionTest, DecayResistanceAtZeroIsOne) {
    EXPECT_TRUE(FloatEqual(eidetic_compute_decay_resistance(0.0f), 1.0f));
}

TEST_F(EideticRegressionTest, DecayResistanceAtMaxIsPointOne) {
    EXPECT_TRUE(FloatEqual(eidetic_compute_decay_resistance(3.0f), 0.1f));
}

TEST_F(EideticRegressionTest, DecayResistanceMonotonicallyDecreasing) {
    float prev = eidetic_compute_decay_resistance(0.0f);
    for (float strength = 0.5f; strength <= 3.0f; strength += 0.5f) {
        float current = eidetic_compute_decay_resistance(strength);
        EXPECT_LT(current, prev);
        prev = current;
    }
}

//=============================================================================
// 4. ERROR CODE CONSISTENCY TESTS
//=============================================================================

TEST_F(EideticRegressionTest, ErrorCodeSuccessIsZero) {
    EXPECT_EQ(EIDETIC_SUCCESS, 0);
}

TEST_F(EideticRegressionTest, ErrorStringSuccessContent) {
    EXPECT_STREQ(eidetic_error_string(EIDETIC_SUCCESS), "Success");
}

TEST_F(EideticRegressionTest, ErrorStringNullPointerContent) {
    EXPECT_STREQ(eidetic_error_string(EIDETIC_ERROR_NULL_POINTER), "Null pointer argument");
}

TEST_F(EideticRegressionTest, ErrorStringInvalidConfigContent) {
    EXPECT_STREQ(eidetic_error_string(EIDETIC_ERROR_INVALID_CONFIG), "Invalid eidetic configuration");
}

TEST_F(EideticRegressionTest, ErrorStringSystemNotSupportedContent) {
    EXPECT_STREQ(eidetic_error_string(EIDETIC_ERROR_SYSTEM_NOT_SUPPORTED), "Memory system not supported");
}

TEST_F(EideticRegressionTest, ErrorStringApplyFailedContent) {
    EXPECT_STREQ(eidetic_error_string(EIDETIC_ERROR_APPLY_FAILED), "Failed to apply eidetic enhancement");
}

TEST_F(EideticRegressionTest, ErrorStringAlreadyAppliedContent) {
    EXPECT_STREQ(eidetic_error_string(EIDETIC_ERROR_ALREADY_APPLIED), "Eidetic enhancement already applied");
}

TEST_F(EideticRegressionTest, ErrorStringUnknownReturnsUnknown) {
    EXPECT_STREQ(eidetic_error_string((eidetic_error_t)999), "Unknown error");
}

//=============================================================================
// 5. FIELD BOUNDS CONSISTENCY TESTS
//=============================================================================

TEST_F(EideticRegressionTest, PresetHippocampusEncodingSpeedInRange) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);

    // Encoding speed should be in sensible range [1.0, 10.0]
    EXPECT_GE(tesla->hippocampus.encoding_speed, 1.0f);
    EXPECT_LE(tesla->hippocampus.encoding_speed, 10.0f);

    // Encoding fidelity should be in range [0.0, 1.0]
    EXPECT_GE(tesla->hippocampus.encoding_fidelity, 0.0f);
    EXPECT_LE(tesla->hippocampus.encoding_fidelity, 1.0f);
}

TEST_F(EideticRegressionTest, PresetWorkingMemoryCapacityBoostInRange) {
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    ASSERT_NE(vn, nullptr);

    // Capacity boost should be in sensible range [0, 10]
    EXPECT_LE(vn->working_memory.capacity_boost, 10u);
}

TEST_F(EideticRegressionTest, PresetDecayMultiplierInRange) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);

    // Decay multiplier should be in range (0, 1]
    EXPECT_GT(tesla->working_memory.decay_multiplier, 0.0f);
    EXPECT_LE(tesla->working_memory.decay_multiplier, 1.0f);
}

TEST_F(EideticRegressionTest, PresetAutobiographicalCapacityInRange) {
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    ASSERT_NE(kp, nullptr);

    // Autobiographical capacity multiplier should be in sensible range [1, 20]
    EXPECT_GE(kp->autobiographical.capacity_multiplier, 1u);
    EXPECT_LE(kp->autobiographical.capacity_multiplier, 20u);
}

TEST_F(EideticRegressionTest, PresetSemanticCapacityInRange) {
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    ASSERT_NE(kp, nullptr);

    // Semantic concept capacity multiplier should be in sensible range [1, 16]
    EXPECT_GE(kp->semantic.concept_capacity_multiplier, 1u);
    EXPECT_LE(kp->semantic.concept_capacity_multiplier, 16u);
}

//=============================================================================
// 6. DETERMINISM TESTS
//=============================================================================

TEST_F(EideticRegressionTest, PresetCallsReturnSamePointer) {
    // Preset functions should return the same static pointer
    const eidetic_memory_config_t* t1 = eidetic_config_tesla();
    const eidetic_memory_config_t* t2 = eidetic_config_tesla();
    EXPECT_EQ(t1, t2);

    const eidetic_memory_config_t* m1 = eidetic_config_mozart();
    const eidetic_memory_config_t* m2 = eidetic_config_mozart();
    EXPECT_EQ(m1, m2);
}

TEST_F(EideticRegressionTest, ScaleValueDeterministic) {
    // Same inputs should always produce same output
    for (int i = 0; i < 10; i++) {
        float result = eidetic_scale_value(100.0f, 2.0f, 4.0f);
        EXPECT_TRUE(FloatEqual(result, 300.0f));
    }
}

TEST_F(EideticRegressionTest, DecayResistanceDeterministic) {
    // Same input should always produce same output
    for (int i = 0; i < 10; i++) {
        float result = eidetic_compute_decay_resistance(1.5f);
        EXPECT_TRUE(FloatEqual(result, eidetic_compute_decay_resistance(1.5f)));
    }
}
