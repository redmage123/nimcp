/**
 * @file test_eidetic_memory.cpp
 * @brief Unit tests for eidetic memory integration module
 *
 * Test Categories:
 * 1. Validation Tests - Config validation, error strings
 * 2. Utility Function Tests - Scale values, decay resistance
 * 3. Per-System Apply Tests - Individual memory system enhancement
 * 4. Master Apply Tests - Apply to all systems
 * 5. Preset Tests - Tesla, Mozart, von Neumann, Kim Peek, Wiltshire configs
 * 6. Error Handling Tests - NULL pointers, invalid configs
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

class EideticMemoryTest : public ::testing::Test {
protected:
    eidetic_memory_config_t config;

    void SetUp() override {
        // Initialize with Tesla preset (visual-spatial dominant)
        const eidetic_memory_config_t* tesla = eidetic_config_tesla();
        ASSERT_NE(tesla, nullptr);
        memcpy(&config, tesla, sizeof(eidetic_memory_config_t));
    }

    void TearDown() override {
        // No cleanup needed
    }

    // Helper to create minimal valid config
    void CreateMinimalConfig() {
        memset(&config, 0, sizeof(config));
        config.visual_eidetic = 1.0f;
        config.auditory_eidetic = 1.0f;
        config.spatial_eidetic = 1.0f;
        config.verbal_eidetic = 1.0f;
        config.encoding_speed = 1.0f;
        config.decay_resistance = 1.0f;
        config.retrieval_accuracy = 0.9f;
        config.detail_granularity = 1.0f;
    }
};

//=============================================================================
// 1. VALIDATION TESTS
//=============================================================================

TEST_F(EideticMemoryTest, ValidConfigReturnsTrue) {
    EXPECT_TRUE(eidetic_config_is_valid(&config));
}

TEST_F(EideticMemoryTest, NullConfigReturnsFalse) {
    EXPECT_FALSE(eidetic_config_is_valid(nullptr));
}

TEST_F(EideticMemoryTest, NegativeVisualEideticInvalid) {
    config.visual_eidetic = -0.1f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticMemoryTest, ExcessiveVisualEideticInvalid) {
    config.visual_eidetic = 3.1f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticMemoryTest, NegativeAuditoryEideticInvalid) {
    config.auditory_eidetic = -0.5f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticMemoryTest, ExcessiveSpatialEideticInvalid) {
    config.spatial_eidetic = 4.0f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticMemoryTest, NegativeEncodingSpeedInvalid) {
    config.encoding_speed = -1.0f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticMemoryTest, RetrievalAccuracyOutOfRangeInvalid) {
    config.retrieval_accuracy = 1.5f;
    EXPECT_FALSE(eidetic_config_is_valid(&config));
}

TEST_F(EideticMemoryTest, BoundaryValuesValid) {
    // Test boundary values (exactly 0.0 and 3.0)
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
// 2. UTILITY FUNCTION TESTS
//=============================================================================

TEST_F(EideticMemoryTest, ScaleValueAtZeroStrength) {
    float result = eidetic_scale_value(100.0f, 0.0f, 4.0f);
    EXPECT_FLOAT_EQ(result, 100.0f);  // No scaling at strength 0
}

TEST_F(EideticMemoryTest, ScaleValueAtMaxStrength) {
    float result = eidetic_scale_value(100.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(result, 400.0f);  // 4x at max strength
}

TEST_F(EideticMemoryTest, ScaleValueAtMidStrength) {
    float result = eidetic_scale_value(100.0f, 1.5f, 4.0f);
    // At strength 1.5 (half of 3.0), multiplier should be 1.0 + (4.0-1.0)*0.5 = 2.5
    EXPECT_FLOAT_EQ(result, 250.0f);
}

TEST_F(EideticMemoryTest, ScaleValueWithDifferentBase) {
    float result = eidetic_scale_value(50.0f, 3.0f, 2.0f);
    EXPECT_FLOAT_EQ(result, 100.0f);  // 2x at max strength
}

TEST_F(EideticMemoryTest, DecayResistanceAtZeroStrength) {
    float result = eidetic_compute_decay_resistance(0.0f);
    EXPECT_FLOAT_EQ(result, 1.0f);  // Normal decay at strength 0
}

TEST_F(EideticMemoryTest, DecayResistanceAtMaxStrength) {
    float result = eidetic_compute_decay_resistance(3.0f);
    EXPECT_FLOAT_EQ(result, 0.1f);  // 10x slower decay at max strength
}

TEST_F(EideticMemoryTest, DecayResistanceAtMidStrength) {
    float result = eidetic_compute_decay_resistance(1.5f);
    // Exponential relationship: 0.1^(1.5/3.0) = 0.1^0.5 = sqrt(0.1) ≈ 0.316
    EXPECT_NEAR(result, 0.316f, 0.01f);
}

TEST_F(EideticMemoryTest, DecayResistanceMonotonic) {
    // Decay resistance should decrease as strength increases
    float r1 = eidetic_compute_decay_resistance(0.5f);
    float r2 = eidetic_compute_decay_resistance(1.0f);
    float r3 = eidetic_compute_decay_resistance(2.0f);

    EXPECT_GT(r1, r2);
    EXPECT_GT(r2, r3);
}

//=============================================================================
// 3. ERROR STRING TESTS
//=============================================================================

TEST_F(EideticMemoryTest, ErrorStringSuccess) {
    const char* str = eidetic_error_string(EIDETIC_SUCCESS);
    EXPECT_STREQ(str, "Success");
}

TEST_F(EideticMemoryTest, ErrorStringNullPointer) {
    const char* str = eidetic_error_string(EIDETIC_ERROR_NULL_POINTER);
    EXPECT_STREQ(str, "Null pointer argument");
}

TEST_F(EideticMemoryTest, ErrorStringInvalidConfig) {
    const char* str = eidetic_error_string(EIDETIC_ERROR_INVALID_CONFIG);
    EXPECT_STREQ(str, "Invalid eidetic configuration");
}

TEST_F(EideticMemoryTest, ErrorStringUnknown) {
    const char* str = eidetic_error_string((eidetic_error_t)999);
    EXPECT_STREQ(str, "Unknown error");
}

//=============================================================================
// 4. PER-SYSTEM APPLY TESTS (with NULL systems - should return error)
//=============================================================================

TEST_F(EideticMemoryTest, ApplyToWorkingMemoryNullSystem) {
    eidetic_error_t err = eidetic_apply_to_working_memory(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToWorkingMemoryNullConfig) {
    // We don't have actual working_memory_t, but we can test null config
    // When system is also null, null pointer error takes precedence
    eidetic_error_t err = eidetic_apply_to_working_memory(nullptr, nullptr);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToHippocampusNullSystem) {
    eidetic_error_t err = eidetic_apply_to_hippocampus(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToSemanticNullSystem) {
    eidetic_error_t err = eidetic_apply_to_semantic(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToConsolidationNullSystem) {
    eidetic_error_t err = eidetic_apply_to_consolidation(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToEngramNullSystem) {
    eidetic_error_t err = eidetic_apply_to_engram(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToHopfieldNullSystem) {
    eidetic_error_t err = eidetic_apply_to_hopfield(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToProceduralNullSystem) {
    eidetic_error_t err = eidetic_apply_to_procedural(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToProspectiveNullSystem) {
    eidetic_error_t err = eidetic_apply_to_prospective(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToAutobiographicalNullSystem) {
    eidetic_error_t err = eidetic_apply_to_autobiographical(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

//=============================================================================
// 5. MASTER APPLY TESTS
//=============================================================================

TEST_F(EideticMemoryTest, ApplyToAllNullConfig) {
    eidetic_error_t err = eidetic_apply_to_all(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyToAllNoSystems) {
    // Valid config but no systems
    eidetic_error_t err = eidetic_apply_to_all(&config, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);  // No systems available
}

TEST_F(EideticMemoryTest, ApplyToAllInvalidConfig) {
    config.visual_eidetic = -1.0f;  // Invalid
    eidetic_error_t err = eidetic_apply_to_all(&config, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, EIDETIC_ERROR_INVALID_CONFIG);
}

//=============================================================================
// 6. PRESET TESTS
//=============================================================================

TEST_F(EideticMemoryTest, TeslaPresetValid) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    ASSERT_NE(tesla, nullptr);
    EXPECT_TRUE(eidetic_config_is_valid(tesla));

    // Tesla was visual-spatial dominant
    EXPECT_GT(tesla->visual_eidetic, 2.0f);
    EXPECT_GT(tesla->spatial_eidetic, 2.0f);
}

TEST_F(EideticMemoryTest, MozartPresetValid) {
    const eidetic_memory_config_t* mozart = eidetic_config_mozart();
    ASSERT_NE(mozart, nullptr);
    EXPECT_TRUE(eidetic_config_is_valid(mozart));

    // Mozart was auditory dominant
    EXPECT_GT(mozart->auditory_eidetic, 2.0f);
}

TEST_F(EideticMemoryTest, VonNeumannPresetValid) {
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    ASSERT_NE(vn, nullptr);
    EXPECT_TRUE(eidetic_config_is_valid(vn));

    // von Neumann was numerical/verbal dominant
    EXPECT_GT(vn->verbal_eidetic, 2.0f);
}

TEST_F(EideticMemoryTest, KimPeekPresetValid) {
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    ASSERT_NE(kp, nullptr);
    EXPECT_TRUE(eidetic_config_is_valid(kp));

    // Kim Peek had encyclopedic factual recall
    EXPECT_GT(kp->verbal_eidetic, 2.0f);
    EXPECT_GT(kp->detail_granularity, 2.0f);
}

TEST_F(EideticMemoryTest, WiltshirePresetValid) {
    const eidetic_memory_config_t* sw = eidetic_config_wiltshire();
    ASSERT_NE(sw, nullptr);
    EXPECT_TRUE(eidetic_config_is_valid(sw));

    // Stephen Wiltshire is visual-artistic
    EXPECT_GT(sw->visual_eidetic, 2.0f);
}

TEST_F(EideticMemoryTest, PresetsHaveDifferentProfiles) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    const eidetic_memory_config_t* mozart = eidetic_config_mozart();
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();

    // Each should have different dominant modality
    EXPECT_GT(tesla->visual_eidetic, mozart->visual_eidetic);
    EXPECT_GT(mozart->auditory_eidetic, tesla->auditory_eidetic);
    EXPECT_GT(vn->verbal_eidetic, tesla->verbal_eidetic);
}

//=============================================================================
// 7. CONFIG FIELD TESTS
//=============================================================================

TEST_F(EideticMemoryTest, WorkingMemoryConfigFields) {
    // Check that working memory config has expected fields
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    EXPECT_GT(tesla->working_memory.capacity_boost, 0u);
    EXPECT_GT(tesla->working_memory.decay_multiplier, 0.0f);
    EXPECT_LE(tesla->working_memory.decay_multiplier, 1.0f);
}

TEST_F(EideticMemoryTest, HippocampusConfigFields) {
    const eidetic_memory_config_t* tesla = eidetic_config_tesla();
    // Tesla preset sets encoding_speed and encoding_fidelity
    EXPECT_GT(tesla->hippocampus.encoding_speed, 1.0f);
    EXPECT_GT(tesla->hippocampus.encoding_fidelity, 0.9f);
    EXPECT_TRUE(tesla->hippocampus.single_exposure_learning);
}

TEST_F(EideticMemoryTest, SemanticConfigFields) {
    const eidetic_memory_config_t* vn = eidetic_config_vonneumann();
    // von Neumann preset sets concept_capacity_multiplier and enable_instant_learning
    EXPECT_GT(vn->semantic.concept_capacity_multiplier, 1u);
    EXPECT_TRUE(vn->semantic.enable_instant_learning);
}

TEST_F(EideticMemoryTest, AutobiographicalConfigFields) {
    const eidetic_memory_config_t* kp = eidetic_config_kim_peek();
    // Kim Peek preset sets autobiographical memory fields
    EXPECT_GT(kp->autobiographical.capacity_multiplier, 1u);
    EXPECT_TRUE(kp->autobiographical.enable_flashbulb_mode);
}

//=============================================================================
// 8. INVALID CONFIG APPLY TESTS
//=============================================================================

TEST_F(EideticMemoryTest, ApplyInvalidConfigToWorkingMemory) {
    config.visual_eidetic = -1.0f;  // Invalid
    eidetic_error_t err = eidetic_apply_to_working_memory(nullptr, &config);
    // NULL system takes precedence
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}

TEST_F(EideticMemoryTest, ApplyInvalidConfigToHippocampus) {
    config.spatial_eidetic = 5.0f;  // Invalid (> 3.0)
    eidetic_error_t err = eidetic_apply_to_hippocampus(nullptr, &config);
    EXPECT_EQ(err, EIDETIC_ERROR_NULL_POINTER);
}
